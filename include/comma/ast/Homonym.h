//===-- ast/Homonym.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// A Homonym represents a set of directly visible declarations associated with a
// given identifier.  These objects are used to implement lookup resolution.
//
// Every declaration associated with a homonym has a visibility attribuite.  We
// have:
//
//   - immediate visibility -- lexically scoped declarations such as function
//     formal parameters, object declarations, etc.
//
//   - import visibility -- declarations which are introduced into a scope by
//     way of a 'import' clause.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_HOMONYM_HDR_GUARD
#define COMMA_AST_HOMONYM_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "llvm/ADT/PointerIntPair.h"
#include <list>

namespace comma {

class Homonym {

    typedef std::list<Decl*> DeclList;

    DeclList directDecls;
    DeclList importDecls;

public:
    Homonym() { }

    ~Homonym() { clear(); }

    void clear() {
        directDecls.clear();
        importDecls.clear();
    }

    void addDirectDecl(Decl *decl) {
        directDecls.push_front(decl);
    }

    void addImportDecl(Decl *decl) {
        importDecls.push_front(decl);
    }

    // Returns true if this Homonym is empty.
    bool empty() const {
        return directDecls.empty() && importDecls.empty();
    }

    // Returns true if this Homonym contains import declarations.
    bool hasImportDecls() const {
        return !importDecls.empty();
    }

    // Returns true if this Homonym contains direct declarations.
    bool hasDirectDecls() const {
        return !directDecls.empty();
    }

    typedef DeclList::iterator DirectIterator;
    typedef DeclList::iterator ImportIterator;

    DirectIterator beginDirectDecls() { return directDecls.begin(); }

    DirectIterator endDirectDecls() { return directDecls.end(); }

    ImportIterator beginImportDecls() { return importDecls.begin(); }

    ImportIterator endImportDecls() { return importDecls.end(); }

    void eraseDirectDecl(DirectIterator &iter) { directDecls.erase(iter); }

    void eraseImportDecl(ImportIterator &iter) { importDecls.erase(iter); }
};

} // End comma namespace.

#endif
