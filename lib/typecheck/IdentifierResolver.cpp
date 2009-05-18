//===-- typecheck/IdentifierResolver.cpp ---------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "IdentifierResolver.h"
#include "comma/ast/Decl.h"
#include "llvm/Support/Casting.h"
#include <algorithm>


using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


bool IdentifierResolver::ArityPred::operator()(const Decl* decl) const {
    if (const SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(decl))
        return sdecl->getArity() != arity;
    if (arity == 0)
        return !isa<EnumLiteral>(decl);
    return false;
}

bool IdentifierResolver::NullaryPred::operator()(const Decl* decl) const {
    if (const SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(decl))
        return sdecl->getArity() == 0;
    return true;
}

unsigned IdentifierResolver::numResolvedDecls() const {
    unsigned result = 0;
    result += hasDirectValue();
    result += numDirectOverloads();
    result += numIndirectValues();
    result += numIndirectOverloads();
    return result;
}

bool IdentifierResolver::filterOverloadsWRTArity(unsigned arity)
{
    return filterOverloads(ArityPred(arity));
}

bool IdentifierResolver::filterProcedures()
{
    return filterOverloads(TypePred<ProcedureDecl>());
}

bool IdentifierResolver::filterFunctionals()
{
    // FIXME: We should probably do this more efficiently.
    return filterOverloads(TypePred<FunctionDecl>()) ||
        filterOverloads(TypePred<EnumLiteral>());
}

bool IdentifierResolver::filterNullaryOverloads()
{
    return filterOverloads(NullaryPred());
}

bool IdentifierResolver::resolve(IdentifierInfo *idInfo)
{
    Homonym *homonym = idInfo->getMetadata<Homonym>();
    if (!homonym || homonym->empty())
        return false;

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
                    if (!(duplicated = stype->equals(targetType)))
                        break;
                }
            }
            if (!duplicated) directOverloads.push_back(sdecl);
        }
        else if (EnumLiteral *elit = dyn_cast<EnumLiteral>(candidate))
            directOverloads.push_back(elit);
        else if (ValueDecl *vdecl = dyn_cast<ValueDecl>(candidate)) {
            if (directOverloads.empty()) {
                directValue = vdecl;
                return true;
            }
            break;
        }
    }

    // Scan the full set of imported declarations, and partition the
    // import decls into two sets:  one containing all value declarations, the
    // other containing all nullary function declarations.
    for (Homonym::ImportIterator iter = homonym->beginImportDecls();
         iter != homonym->endImportDecls(); ++iter) {
        Decl *candidate = *iter;
        if (SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(candidate))
            indirectOverloads.push_back(sdecl);
        else if (EnumLiteral *elit = dyn_cast<EnumLiteral>(candidate))
            indirectOverloads.push_back(elit);
        else if (ValueDecl *vdecl = dyn_cast<ValueDecl>(candidate))
            indirectValues.push_back(vdecl);
    }

    return numResolvedDecls() != 0;
}
