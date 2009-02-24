//===-- ast/Homonym.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// A Homonym represents a set of directly visible declarations associated or
// bound to the same identifier and are used to implement lookup resolution.
//
// Homonyms can be singletons -- that is, there is only one associated
// declaration.  In this case, the homonym is morally equivalent to the
// declaration itself.  Or, a homonym can contain several declarations, in which
// case we say the homonym is loaded.
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
#include "llvm/ADT/SmallVector.h"

namespace comma {

class Homonym {

    typedef llvm::SmallVector<Decl*, 2> DeclVector;

    // When a homonym is not a singleton (that is, it contains two or more
    // declarations), a VisibilitySet is constructed to hold the various decls,
    // partitioned according to their visibility.
    struct VisibilitySet {
        DeclVector directDecls;
        DeclVector importDecls;
    };

    // Homonyms are not meant to be heap allocated.  We want the size of this
    // class to fit in a single word.  We use an llvm::PtrIntPair to represent
    // what kind of homonym we have.  There are four cases:
    //
    //   1) A singleton homonym which names a direct declaration.  The
    //   representation is a pointer to the declaration node.
    //
    //   2) A singleton homonym which names an imported declaration.  Again, the
    //   representation is a pointer to the declaration node.
    //
    //   3) A loaded homonym consists of two or more declarations.  In this
    //   case, the representation is a pointer to a heap allocated
    //   VisibilitySet.
    //
    //   4) A homonym can be empty.  This is useful when implementing scoped
    //   lookup where an identifier can become unbound.
    //
    // The three cases above are identified with the following enumeration.
    enum HomonymKind {
        HOMONYM_DIRECT,
        HOMONYM_IMPORT,
        HOMONYM_LOADED,
        HOMONYM_EMPTY
    };

    // The representation is via an llvm::PointerIntPair, where the tag bit takes on
    // one of the values from the above enumeration.
    typedef llvm::PointerIntPair<void*, 2, HomonymKind> Representation;
    Representation rep;

    VisibilitySet *asVisibilitySet() {
        assert(isLoaded() &&
               "Cannot convert singleton homonym to visibility set!");
        return static_cast<VisibilitySet*>(rep.getPointer());
    }

    // If the representation is not loaded, creates a new visibility set and
    // adjusts the representation, otherwise returns the set already associated
    // with this Homonym.
    VisibilitySet *getOrCreateVisibilitySet();

public:
    Homonym() : rep(0, HOMONYM_EMPTY) { }

    ~Homonym() { clear(); }

    void clear() {
        if (isLoaded()) delete asVisibilitySet();
        rep.setInt(HOMONYM_EMPTY);
        rep.setPointer(0);
    }

    void addDirectDecl(Decl *decl) {
        if (isEmpty()) {
            rep.setPointer(decl);
            rep.setInt(HOMONYM_DIRECT);
        }
        else {
            VisibilitySet *vset = getOrCreateVisibilitySet();
            vset->directDecls.push_back(decl);
        }
    }

    void addImportDecl(Decl *decl) {
        if (isEmpty()) {
            rep.setPointer(decl);
            rep.setInt(HOMONYM_IMPORT);
        }
        else {
            VisibilitySet *vset = getOrCreateVisibilitySet();
            vset->importDecls.push_back(decl);
        }
    }

    // Several predicates to test what kind of Homonym this is.
    bool isDirectSingleton() const { return rep.getInt() == HOMONYM_DIRECT; }

    bool isImportSingleton() const { return rep.getInt() == HOMONYM_IMPORT; }

    bool isLoaded() const { return rep.getInt() == HOMONYM_LOADED; }

    bool isEmpty() const { return rep.getInt() == HOMONYM_EMPTY; }

    bool isSingleton() const {
        return isDirectSingleton() || isImportSingleton();
    }

    // Convert this Homonym to the corresponding declaration node.  This homonym
    // must be a singleton.
    Decl *asDeclaration() {
        assert(isSingleton() &&
               "Cannot convert loaded homonym to a single decl!");
        return static_cast<Decl*>(rep.getPointer());
    }

    class DirectIterator {

        DeclVector::iterator cursor;

        DirectIterator(const DeclVector::iterator &iter)
            : cursor(iter) { }

        friend class Homonym;

    public:
        DirectIterator(const DirectIterator &iter)
            : cursor(iter.cursor) { }

        Decl *operator *() { return *(cursor - 1); }

        bool operator ==(const DirectIterator &iter) const {
            return this->cursor == iter.cursor;
        }

        bool operator !=(const DirectIterator &iter) const {
            return this->cursor != iter.cursor;
        }

        DirectIterator &operator ++() {
            --cursor;
            return *this;
        }

        DirectIterator operator ++(int) {
            DirectIterator tmp = *this;
            --cursor;
            return tmp;
        }

        DirectIterator &operator --() {
            ++cursor;
            return *this;
        }

        DirectIterator operator --(int) {
            DirectIterator tmp = *this;
            ++cursor;
            return tmp;
        }
    };

    DirectIterator beginDirectDecls() {
        assert(isLoaded() && "Cannot iterate over singleton homonyms.");
        return DirectIterator(asVisibilitySet()->directDecls.end());
    }

    DirectIterator endDirectDecls() {
        assert(isLoaded() && "Cannot iterate over singleton homonyms.");
        return DirectIterator(asVisibilitySet()->directDecls.begin());
    }

    typedef DeclVector::iterator ImportIterator;
    ImportIterator beginImportDecls() {
        assert(isLoaded() && "Cannot iterate over singleton homonyms.");
        return asVisibilitySet()->importDecls.begin();
    }

    ImportIterator endImportDecls() {
        assert(isLoaded() && "Cannot iterate over singleton homonyms.");
        return asVisibilitySet()->importDecls.end();
    }

    void eraseDirectDecl(DirectIterator &iter) {
        assert(isLoaded() && "Cannot erase singleton homonyms.");
        asVisibilitySet()->directDecls.erase(iter.cursor - 1);
    }

    void eraseImportDecl(ImportIterator &iter) {
        assert(isLoaded() && "Cannot erase singleton homonyms.");
        asVisibilitySet()->importDecls.erase(iter);
    }
};

} // End comma namespace.

#endif
