//===-- typecheck/Resolver.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Homonym.h"
#include "Resolver.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void Resolver::clear()
{
    idInfo = 0;
    directDecl = 0;
    directOverloads.clear();
    indirectValues.clear();
    indirectTypes.clear();
    indirectOverloads.clear();
    indirectExceptions.clear();
}

bool Resolver::ArityPred::operator()(const Decl* decl) const {
    if (const SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(decl))
        return sdecl->getArity() != arity;
    if (arity == 0)
        return !isa<EnumLiteral>(decl);
    return false;
}

bool Resolver::NullaryPred::operator()(const Decl* decl) const {
    if (const SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(decl))
        return sdecl->getArity() == 0;
    return true;
}

unsigned Resolver::numResolvedDecls() const {
    unsigned result = 0;
    result += hasDirectValue();
    result += hasDirectType();
    result += numDirectOverloads();
    result += numIndirectValues();
    result += numIndirectTypes();
    result += numIndirectOverloads();
    result += numIndirectExceptions();
    return result;
}

bool Resolver::filterOverloadsWRTArity(unsigned arity)
{
    return filterOverloads(ArityPred(arity));
}

bool Resolver::filterProcedures()
{
    return filterOverloads(TypePred<ProcedureDecl>());
}

bool Resolver::filterFunctionals()
{
    // FIXME: We should probably do this more efficiently.
    return filterOverloads(TypePred<FunctionDecl>()) ||
        filterOverloads(TypePred<EnumLiteral>());
}

bool Resolver::filterNullaryOverloads()
{
    return filterOverloads(NullaryPred());
}

bool Resolver::resolveDirectDecls(Homonym *homonym)
{
    for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
         iter != homonym->endDirectDecls(); ++iter) {
        Decl *candidate = *iter;
        if (SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(candidate)) {
            SubroutineType *stype = sdecl->getType();
            bool duplicated = false;
            for (unsigned i = 0; i < directOverloads.size(); ++i) {
                if (SubroutineDecl *targetDecl =
                    dyn_cast<SubroutineDecl>(directOverloads[i])) {
                    SubroutineType *targetType = targetDecl->getType();
                    if (!(duplicated = stype == targetType))
                        break;
                }
            }
            if (!duplicated)
                directOverloads.push_back(sdecl);
        }
        else {
            assert((isa<ValueDecl>(candidate)     ||
                    isa<TypeDecl>(candidate)      ||
                    isa<ModelDecl>(candidate)     ||
                    isa<PackageDecl>(candidate)   ||
                    isa<ExceptionDecl>(candidate) ||
                    isa<ComponentDecl>(candidate)) &&
                   "Bad type of direct declaration!");
            if (directOverloads.empty()) {
                directDecl = candidate;
                return true;
            }
            break;
        }
    }
    return !directOverloads.empty();
}

bool Resolver::resolveIndirectDecls(Homonym *homonym)
{
    if (!homonym->hasImportDecls())
        return false;

    // Scan the set of indirect declarations associcated with the homonym and
    // partition them into four sets corresponding to the accessible values,
    // subroutines, types, and exceptions.
    for (Homonym::ImportIterator iter = homonym->beginImportDecls();
         iter != homonym->endImportDecls(); ++iter) {
        Decl *candidate = *iter;
        if (SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(candidate))
            indirectOverloads.push_back(sdecl);
        else if (ValueDecl *vdecl = dyn_cast<ValueDecl>(candidate))
            indirectValues.push_back(vdecl);
        else if (TypeDecl *tdecl = dyn_cast<TypeDecl>(candidate))
            indirectTypes.push_back(tdecl);
        else if (ExceptionDecl *edecl = dyn_cast<ExceptionDecl>(candidate))
            indirectExceptions.push_back(edecl);
        else
            assert(false && "Bad type of indirect declaration!");
    }
    return true;
}

bool Resolver::resolve(IdentifierInfo *idInfo)
{
    Homonym *homonym = idInfo->getMetadata<Homonym>();

    this->idInfo = idInfo;
    if (!homonym || homonym->empty())
        return false;
    return resolveDirectDecls(homonym) | resolveIndirectDecls(homonym);
}

bool Resolver::getVisibleSubroutines(
    llvm::SmallVectorImpl<SubroutineDecl*> &srDecls)
{
    // If there are any direct values, no subroutines are visible.
    if (hasDirectValue())
        return false;

    unsigned numEntries = srDecls.size();

    // Resolve all direct subroutines.
    direct_overload_iter DE = end_direct_overloads();
    for (direct_overload_iter I = begin_direct_overloads(); I != DE; ++I)
        if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*I))
            srDecls.push_back(SR);

    // If there are any indirect values, we are done.
    if (hasIndirectValues())
        return numEntries != srDecls.size();

    // Resolve all indirect subroutines.
    indirect_overload_iter IE = end_indirect_overloads();
    for (indirect_overload_iter I = begin_indirect_overloads(); I != IE; ++I)
        if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*I))
            srDecls.push_back(SR);

    return numEntries != srDecls.size();
}
