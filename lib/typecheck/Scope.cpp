//===-- typecheck/Scope.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Homonym.h"
#include "Scope.h"
#include "comma/basic/IdentifierInfo.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"

#include "llvm/Support/Casting.h"
#include "llvm/Support/DataTypes.h"

#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Scope::Entry method.

bool Scope::Entry::containsImportDecl(DomainType *type)
{
    ImportIterator endIter = endImportDecls();

    for (ImportIterator iter = beginImportDecls(); iter != endIter; ++iter)
        if (type == *iter) return true;
    return false;
}

bool Scope::Entry::containsImportDecl(IdentifierInfo *name)
{
    ImportIterator endIter = endImportDecls();

    for (ImportIterator iter = beginImportDecls(); iter != endIter; ++iter)
        if (name == (*iter)->getIdInfo()) return true;
    return false;
}

void Scope::Entry::addDirectDecl(Decl *decl)
{
    if (directDecls.insert(decl)) {
        IdentifierInfo *idInfo  = decl->getIdInfo();
        Homonym        *homonym = getOrCreateHomonym(idInfo);
        homonym->addDirectDecl(decl);
    }
}

void Scope::Entry::removeDirectDecl(Decl *decl)
{
    IdentifierInfo *info    = decl->getIdInfo();
    Homonym        *homonym = info->getMetadata<Homonym>();
    assert(homonym && "No identifier metadata!");

    for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
         iter != homonym->endDirectDecls(); ++iter)
        if (decl == *iter) {
            homonym->eraseDirectDecl(iter);
            return;
        }
    assert(false && "Decl not associated with corresponding identifier!");
}

void Scope::Entry::removeImportDecl(Decl *decl)
{
    IdentifierInfo *info    = decl->getIdInfo();
    Homonym        *homonym = info->getMetadata<Homonym>();
    assert(homonym && "No identifier metadata!");

    for (Homonym::ImportIterator iter = homonym->beginImportDecls();
         iter != homonym->endImportDecls(); ++iter)
        if (decl == *iter) {
            homonym->eraseImportDecl(iter);
            return;
        }
    assert(false && "Decl not associated with corresponding indentifier!");
}

bool Scope::Entry::containsDirectDecl(IdentifierInfo *name)
{
    DirectIterator endIter = endDirectDecls();
    for (DirectIterator iter = beginDirectDecls(); iter != endIter; ++iter)
        if (name == (*iter)->getIdInfo()) return true;
    return false;
}

void Scope::Entry::importDeclarativeRegion(DeclRegion *region)
{
    typedef DeclRegion::DeclIter DeclIter;

    DeclIter iter;
    DeclIter endIter = region->endDecls();
    for (iter = region->beginDecls(); iter != endIter; ++iter) {
        Decl           *decl    = *iter;
        IdentifierInfo *idinfo  = decl->getIdInfo();
        Homonym        *homonym = getOrCreateHomonym(idinfo);

        homonym->addImportDecl(decl);

        // Import the contents of enumeration literals.
        if (EnumerationDecl *edecl = dyn_cast<EnumerationDecl>(decl))
            importDeclarativeRegion(edecl);
    }
}

void Scope::Entry::clearDeclarativeRegion(DeclRegion *region)
{
    typedef DeclRegion::DeclIter DeclIter;

    DeclIter iter;
    DeclIter endIter = region->endDecls();
    for (iter = region->beginDecls(); iter != endIter; ++iter) {
        Decl *decl = *iter;
        removeImportDecl(decl);
        // Clear the contents of enumeration literals.
        if (EnumerationDecl *edecl = dyn_cast<EnumerationDecl>(decl))
            clearDeclarativeRegion(edecl);
    }
}

void Scope::Entry::addImportDecl(DomainType *type)
{
    typedef DeclRegion::DeclIter DeclIter;
    DomainTypeDecl *domain = type->getDomainTypeDecl();
    assert(domain && "Cannot import from the given domain!");

    importDeclarativeRegion(domain);
    importDecls.push_back(type);
}

// Turns this into an uninitialized (dead) scope entry.  This method is
// used so that entries can be cached and recognized as inactive objects.
void Scope::Entry::clear()
{
    // Traverse the set of IdentifierInfo's owned by this entry and reduce the
    // associated decl stacks.
    DirectIterator endDeclIter = endDirectDecls();
    for (DirectIterator declIter = beginDirectDecls();
         declIter != endDeclIter; ++declIter)
        removeDirectDecl(*declIter);

    ImportIterator endImportIter = endImportDecls();
    for (ImportIterator importIter = beginImportDecls();
         importIter != endImportIter; ++importIter)
        clearDeclarativeRegion((*importIter)->getDomainTypeDecl());

    kind = DEAD_SCOPE;
    directDecls.clear();
    importDecls.clear();
}

Homonym *Scope::Entry::getOrCreateHomonym(IdentifierInfo *info)
{
    Homonym *homonym = info->getMetadata<Homonym>();

    if (!homonym) {
        homonym = new Homonym();
        info->setMetadata(homonym);
    }
    return homonym;
}

//===----------------------------------------------------------------------===//
// Scope::Resolver methods.

void Scope::Resolver::clear()
{
    directDecl = 0;
    directOverloads.clear();
    indirectValues.clear();
    indirectTypes.clear();
    indirectOverloads.clear();
}

bool Scope::Resolver::ArityPred::operator()(const Decl* decl) const {
    if (const SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(decl))
        return sdecl->getArity() != arity;
    if (arity == 0)
        return !isa<EnumLiteral>(decl);
    return false;
}

bool Scope::Resolver::NullaryPred::operator()(const Decl* decl) const {
    if (const SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(decl))
        return sdecl->getArity() == 0;
    return true;
}

unsigned Scope::Resolver::numResolvedDecls() const {
    unsigned result = 0;
    result += hasDirectValue();
    result += hasDirectType();
    result += numDirectOverloads();
    result += numIndirectValues();
    result += numIndirectTypes();
    result += numIndirectOverloads();
    return result;
}

bool Scope::Resolver::filterOverloadsWRTArity(unsigned arity)
{
    return filterOverloads(ArityPred(arity));
}

bool Scope::Resolver::filterProcedures()
{
    return filterOverloads(TypePred<ProcedureDecl>());
}

bool Scope::Resolver::filterFunctionals()
{
    // FIXME: We should probably do this more efficiently.
    return filterOverloads(TypePred<FunctionDecl>()) ||
        filterOverloads(TypePred<EnumLiteral>());
}

bool Scope::Resolver::filterNullaryOverloads()
{
    return filterOverloads(NullaryPred());
}

bool Scope::Resolver::resolveDirectDecls(Homonym *homonym)
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
                    if (!(duplicated = stype->equals(targetType)))
                        break;
                }
            }
            if (!duplicated) directOverloads.push_back(sdecl);
        }
        else {
            assert((isa<ValueDecl>(candidate) ||
                    isa<TypeDecl>(candidate)  ||
                    isa<ModelDecl>(candidate)) &&
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

bool Scope::Resolver::resolveIndirectDecls(Homonym *homonym)
{
    if (!homonym->hasImportDecls())
        return false;

    // Scan the set of indirect declarations associcated with the homonym and
    // partition them into three sets corresponding to the accessible values,
    // subroutines, and types.
    for (Homonym::ImportIterator iter = homonym->beginImportDecls();
         iter != homonym->endImportDecls(); ++iter) {
        Decl *candidate = *iter;
        if (SubroutineDecl *sdecl = dyn_cast<SubroutineDecl>(candidate))
            indirectOverloads.push_back(sdecl);
        else if (ValueDecl *vdecl = dyn_cast<ValueDecl>(candidate))
            indirectValues.push_back(vdecl);
        else if (TypeDecl *tdecl = dyn_cast<TypeDecl>(candidate))
            indirectTypes.push_back(tdecl);
        else
            assert(false && "Bad type of indirect declaration!");
    }
    return true;
}

bool Scope::Resolver::resolve(IdentifierInfo *idInfo)
{
    Homonym *homonym = idInfo->getMetadata<Homonym>();
    if (!homonym || homonym->empty())
        return false;
    return resolveDirectDecls(homonym) | resolveIndirectDecls(homonym);
}

//===----------------------------------------------------------------------===//
// Scope methods.

Scope::Scope()
    : numCachedEntries(0)
{
    push(CUNIT_SCOPE);
}

Scope::~Scope()
{
    while (!entries.empty()) pop();
}

ScopeKind Scope::getKind() const
{
    return entries.front()->getKind();
}

void Scope::push(ScopeKind kind)
{
    Entry *entry;
    unsigned tag = numEntries() + 1;

    if (numCachedEntries) {
        entry = entryCache[--numCachedEntries];
        entry->initialize(kind, tag);
    }
    else
        entry = new Entry(kind, tag);
    entries.push_front(entry);
}

void Scope::pop()
{
    assert(!entries.empty() && "Cannot pop empty stack frame!");

    Entry *entry = entries.front();

    entry->clear();
    entries.pop_front();
    if (numCachedEntries < ENTRY_CACHE_SIZE)
        entryCache[numCachedEntries++] = entry;
    else
        delete entry;
    return;
}

bool Scope::addImport(DomainType *type)
{
    // First, walk the current stack of frames and check that this type has not
    // already been imported.
    for (EntryStack::const_iterator entryIter = entries.begin();
         entryIter != entries.end(); ++entryIter)
        if ((*entryIter)->containsImportDecl(type)) return true;

    // The import is not yet in scope.  Register it with the current entry.
    entries.front()->addImportDecl(type);

    return false;
}

void Scope::dump() const
{
    std::cerr << "**** Scope trace for <"
              << std::hex << (uintptr_t)this
              << ">:\n";

    unsigned depth = entries.size();
    for (EntryStack::const_iterator entryIter = entries.begin();
         entryIter != entries.end(); ++entryIter) {
        Entry *entry = *entryIter;
        std::cerr << "  Entry[" << depth-- << "] <"
                  << std::hex << (uintptr_t)entry
                  << ">: ";
        switch (entry->getKind()) {
        case BASIC_SCOPE:
            std::cerr << "BASIC_SCOPE\n";
            break;
        case CUNIT_SCOPE:
            std::cerr << "CUINT_SCOPE\n";
            break;
        case MODEL_SCOPE:
            std::cerr << "MODEL_SCOPE\n";
            break;
        case FUNCTION_SCOPE:
            std::cerr << "FUNCTION_SCOPE\n";
            break;
        case DEAD_SCOPE:
            assert(false && "Cannot print uninitialized scope!");
        }

        if (entry->numDirectDecls()) {
            std::cerr << "  Direct Decls:\n";
            for (Entry::DirectIterator lexIter = entry->beginDirectDecls();
                 lexIter != entry->endDirectDecls(); ++lexIter) {
                Decl *decl = *lexIter;
                std::cerr << "   " << decl->getString() << " : ";
                decl->dump();
                std::cerr << '\n';
            }
            std::cerr << '\n';
        }

        if (entry->numImportDecls()) {
            std::cerr << "  Imports:\n";
            for (Entry::ImportIterator importIter = entry->beginImportDecls();
                 importIter != entry->endImportDecls(); ++importIter) {
                DomainType *type = *importIter;
                std::cerr << "   " << type->getString() << " : ";
                type->dump();
                std::cerr << '\n';

                DomainTypeDecl *domain = type->getDomainTypeDecl();
                for (Domoid::DeclIter iter = domain->beginDecls();
                     iter != domain->endDecls(); ++iter) {
                    std::cerr << "      ";
                    (*iter)->dump();
                    std::cerr << '\n';
                }
            }
            std::cerr << '\n';
        }
        std::cerr << std::endl;
    }
}

