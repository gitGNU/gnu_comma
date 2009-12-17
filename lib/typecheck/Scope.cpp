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

        // Import the contents of enumeration and integer decls.
        if (EnumerationDecl *edecl = dyn_cast<EnumerationDecl>(decl))
            importDeclarativeRegion(edecl);
        else if (IntegerDecl *idecl = dyn_cast<IntegerDecl>(decl))
            importDeclarativeRegion(idecl);
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
        // Clear the contents of enumeration and integer decls.
        if (EnumerationDecl *edecl = dyn_cast<EnumerationDecl>(decl))
            clearDeclarativeRegion(edecl);
        else if (IntegerDecl *idecl = dyn_cast<IntegerDecl>(decl))
            clearDeclarativeRegion(idecl);
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

Decl *Scope::addDirectDecl(Decl *decl) {
    assert(!llvm::isa<DomainInstanceDecl>(decl) &&
           "Cannot add domain instance declarations to a scope!");
    if (Decl *conflict = findConflictingDirectDecl(decl))
        return conflict;
    entries.front()->addDirectDecl(decl);
    return 0;
}

void Scope::addDirectDeclNoConflicts(Decl *decl)
{
    assert(!llvm::isa<DomainInstanceDecl>(decl) &&
           "Cannot add domain instance declarations to a scope!");
    assert(findConflictingDirectDecl(decl) == 0 &&
           "Conflicting decl found when there should be none!");
    entries.front()->addDirectDecl(decl);
}

bool Scope::directDeclsConflict(Decl *X, Decl *Y) const
{
    if (X->getIdInfo() != Y->getIdInfo())
        return false;

    // If X denotes a type, model, value, or exception declaration, there is a
    // conflict independent of the type of Y.
    if (isa<ValueDecl>(X) || isa<TypeDecl>(X) ||
        isa<ModelDecl>(X) || isa<ExceptionDecl>(X))
        return true;

    // Similarly, if Y denotes a type, model, value, or exception declaration,
    // there is a conflict with X.
    if (isa<ValueDecl>(Y) || isa<TypeDecl>(Y) ||
        isa<ModelDecl>(Y) || isa<ExceptionDecl>(X))
        return true;

    // Otherwise, X and Y must both denote a subroutine declaration. There is a
    // conflict iff both declarations have the same type.
    SubroutineDecl *XSDecl = cast<SubroutineDecl>(X);
    SubroutineDecl *YSDecl = cast<SubroutineDecl>(Y);
    if (XSDecl->getType() == YSDecl->getType())
        return true;
    return false;
}

Decl *Scope::findConflictingDirectDecl(Decl *candidate) const
{
    Entry *currentEntry = entries.front();

    typedef Entry::DirectIterator iterator;
    iterator E = currentEntry->endDirectDecls();
    for (iterator I = currentEntry->beginDirectDecls(); I != E; ++I) {
        Decl *extant = *I;
        if (directDeclsConflict(extant, candidate))
            return extant;
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
                for (DeclRegion::DeclIter iter = domain->beginDecls();
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

