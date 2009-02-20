//===-- typecheck/Scope.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/IdentifierInfo.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"
#include "comma/typecheck/Scope.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/DataTypes.h"
#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


bool ScopeEntry::containsImportDecl(DomainType *type)
{
    ImportIterator endIter = endImportDecls();

    for (ImportIterator iter = beginImportDecls(); iter != endIter; ++iter)
        if (type == *iter) return true;
    return false;
}

bool ScopeEntry::containsImportDecl(IdentifierInfo *name)
{
    ImportIterator endIter = endImportDecls();

    for (ImportIterator iter = beginImportDecls(); iter != endIter; ++iter)
        if (name == (*iter)->getIdInfo()) return true;
    return false;
}

void ScopeEntry::addDirectDecl(Decl *decl)
{
    if (directDecls.insert(decl)) {
        IdentifierInfo *idInfo  = decl->getIdInfo();
        Homonym        *homonym = getOrCreateHomonym(idInfo);
        homonym->addDirectDecl(decl);
    }
}

void ScopeEntry::removeDirectDecl(Decl *decl)
{
    IdentifierInfo *info    = decl->getIdInfo();
    Homonym        *homonym = info->getMetadata<Homonym>();
    assert(homonym && "No identifier metadata!");

    if (homonym->isSingleton()) {
        homonym->clear();
        return;
    }

    for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
         iter != homonym->endDirectDecls(); ++iter)
        if (decl == *iter) {
            homonym->eraseDirectDecl(iter);
            return;
        }
    assert(false && "Decl not associated with corresponding identifier!");
}

bool ScopeEntry::containsDirectDecl(IdentifierInfo *name)
{
    DirectIterator endIter = endDirectDecls();
    for (DirectIterator iter = beginDirectDecls(); iter != endIter; ++iter)
        if (name == (*iter)->getIdInfo()) return true;
    return false;
}

void ScopeEntry::addImportDecl(DomainType *type)
{
    typedef Domoid::DeclIter DeclIter;
    Domoid *domoid = type->getDomoidDecl();

    assert((isa<AbstractDomainDecl>(domoid) || isa<DomainInstanceDecl>(domoid))
           && "Cannot import from the given domain!");

    DeclIter iter;
    DeclIter endIter = domoid->endDecls();
    for (iter = domoid->beginDecls(); iter != endIter; ++iter) {
        IdentifierInfo *idinfo  = iter->first;
        Decl           *decl    = iter->second;
        Homonym        *homonym = getOrCreateHomonym(idinfo);
        homonym->addImportDecl(decl);
    }

    importDecls.push_back(type);
}

// Traverse the set IdentifierInfo's owned by this entry and reduce the
// associated decl stacks.
void ScopeEntry::clear()
{
    DirectIterator endIter = endDirectDecls();
    for (DirectIterator iter = beginDirectDecls(); iter != endIter; ++iter)
        removeDirectDecl(*iter);

    kind = DEAD_SCOPE;
    directDecls.clear();
    importDecls.clear();
}

Homonym *ScopeEntry::getOrCreateHomonym(IdentifierInfo *info)
{
    Homonym *homonym = info->getMetadata<Homonym>();

    if (!homonym) {
        homonym = new Homonym();
        info->setMetadata(homonym);
    }
    return homonym;
}

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
    ScopeEntry *entry;
    unsigned    tag = numEntries() + 1;

    if (numCachedEntries) {
        entry = entryCache[--numCachedEntries];
        entry->initialize(kind, tag);
    }
    else
        entry = new ScopeEntry(kind, tag);
    entries.push_front(entry);
}

void Scope::pop()
{
    assert(!entries.empty() && "Cannot pop empty stack frame!");

    ScopeEntry *entry = entries.front();

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

ModelDecl *Scope::lookupDirectModel(const IdentifierInfo *name,
                                    bool traverse) const
{
    if (name->hasMetadata()) {
        if (traverse) {
            Homonym *homonym = name->getMetadata<Homonym>();

            if (homonym->isDirectSingleton())
                return dyn_cast<ModelDecl>(homonym->asDeclaration());

            if (homonym->isLoaded()) {
                for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
                     iter != homonym->endDirectDecls(); ++iter) {
                    Decl *candidate = *iter;
                    if (isa<ModelDecl>(candidate))
                        return cast<ModelDecl>(candidate);
                }
            }
        }
        else {
            // Otherwise, scan the direct bindings associated with the current
            // ScopeEntry.  We should never have more than one model associated
            // with an entry, so we need not concern ourselves with the order of
            // the search.
            ScopeEntry *entry = entries.front();
            ScopeEntry::DirectIterator    iter = entry->beginDirectDecls();
            ScopeEntry::DirectIterator endIter = entry->endDirectDecls();
            for ( ; iter != endIter; ++iter) {
                Decl *candidate = *iter;
                if (candidate->getIdInfo() == name && isa<ModelDecl>(candidate))
                    return cast<ModelDecl>(candidate);
            }
        }
    }
    return 0;
}

ValueDecl *Scope::lookupDirectValue(const IdentifierInfo *info) const
{
    if (info->hasMetadata()) {
        Homonym *homonym = info->getMetadata<Homonym>();

        if (homonym->isDirectSingleton())
            return dyn_cast<ValueDecl>(homonym->asDeclaration());

        if (homonym->isLoaded()) {
            for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
                 iter != homonym->endDirectDecls(); ++iter) {
                Decl *candidate = *iter;
                if (isa<ValueDecl>(candidate))
                    return cast<ValueDecl>(candidate);
            }
        }
    }
    return 0;
}

void Scope::dump() const
{
    std::cerr << "**** Scope trace for <"
              << std::hex << (uintptr_t)this
              << ">:\n";

    unsigned depth = entries.size();
    for (EntryStack::const_iterator entryIter = entries.begin();
         entryIter != entries.end(); ++entryIter) {
        ScopeEntry *entry = *entryIter;
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
            unsigned i = 0;
            std::cerr << "  Direct Decls: ";
            for (ScopeEntry::DirectIterator lexIter = entry->beginDirectDecls();
                 lexIter != entry->endDirectDecls(); ++lexIter) {
                if (++i % 5 == 0) std::cerr << "\n                ";
                std::cerr << (*lexIter)->getString() << ' ';
            }
            std::cerr << '\n';
        }

        if (entry->numImportDecls()) {
            unsigned i = 0;
            std::cerr << "  Imports: ";
            for (ScopeEntry::ImportIterator importIter = entry->beginImportDecls();
                 importIter != entry->endImportDecls(); ++importIter) {
                if (++i % 5 == 0) std::cerr << "\n            ";
                std::cerr << (*importIter)->getString() << ' ';
            }
            std::cerr << '\n';
        }
        std::cerr << std::endl;
    }
}

