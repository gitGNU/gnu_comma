//===-- typecheck/Scope.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Homonym.h"
#include "Scope.h"
#include "comma/basic/IdentifierInfo.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"

#include "llvm/Support/Casting.h"

#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Scope::Entry method.

bool Scope::Entry::containsImport(IdentifierInfo *name)
{
    ImportIterator endIter = endImportDecls();

    for (ImportIterator iter = beginImportDecls(); iter != endIter; ++iter)
        if (name == (*iter)->getIdInfo()) return true;
    return false;
}

bool Scope::Entry::containsImport(PkgInstanceDecl *package)
{
    ImportIterator endIter = endImportDecls();

    for (ImportIterator iter = beginImportDecls(); iter != endIter; ++iter)
        if (package == *iter) return true;
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
    assert(containsDirectDecl(decl) &&
           "Declaration not associated with this scope entry!");

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

        switch (decl->getKind())
        {
        default:
            break;

        case Ast::AST_ArrayDecl:
        case Ast::AST_EnumerationDecl:
        case Ast::AST_IntegerDecl:
        case Ast::AST_PrivateTypeDecl:
            importDeclarativeRegion(cast<DeclRegion>(decl));
            break;
        }
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

void Scope::Entry::addImport(PkgInstanceDecl *package)
{
    importDeclarativeRegion(package);
    importDecls.push_back(package);
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
        clearDeclarativeRegion(*importIter);

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

bool Scope::addImport(PkgInstanceDecl *package)
{
    // First, walk the current stack of frames and check that this package has
    // not already been imported.
    for (EntryStack::const_iterator entryIter = entries.begin();
         entryIter != entries.end(); ++entryIter) {
        if ((*entryIter)->containsImport(package))
            return true;
    }

    // The import is not yet in scope.  Register it with the current entry.
    entries.front()->addImport(package);

    return false;
}

Decl *Scope::addDirectDecl(Decl *decl)
{
    if (Decl *conflict = findConflictingDirectDecl(decl))
        return conflict;
    entries.front()->addDirectDecl(decl);
    return 0;
}

void Scope::addDirectDeclNoConflicts(Decl *decl)
{
    assert(findConflictingDirectDecl(decl) == 0 &&
           "Conflicting decl found when there should be none!");
    entries.front()->addDirectDecl(decl);
}

bool Scope::directDeclsConflict(Decl *X, Decl *Y) const
{
    if (X->getIdInfo() != Y->getIdInfo())
        return false;

    // The only type of declarations that can share the same name in the same
    // scope are subroutine declarations (provided that they have distinct
    // types).
    SubroutineDecl *XSDecl = dyn_cast<SubroutineDecl>(X);
    SubroutineDecl *YSDecl = dyn_cast<SubroutineDecl>(Y);
    if (XSDecl && YSDecl)
        return SubroutineDecl::compareProfiles(XSDecl, YSDecl);
    return true;
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
        case PACKAGE_SCOPE:
            std::cerr << "PACKAGE_SCOPE\n";
            break;
        case SUBROUTINE_SCOPE:
            std::cerr << "SUBROUTINE_SCOPE\n";
            break;
        case RECORD_SCOPE:
            std::cerr << "RECORD_SCOPE\n";
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
                Decl *import = *importIter;
                DeclRegion *region = import->asDeclRegion();
                std::cerr << "   " << import->getString() << " : ";
                import->dump();
                std::cerr << '\n';

                for (DeclRegion::DeclIter iter = region->beginDecls();
                     iter != region->endDecls(); ++iter) {
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

