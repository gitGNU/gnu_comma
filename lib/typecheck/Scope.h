//===-- typecheck/Scope.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_SCOPE_HDR_GUARD
#define COMMA_TYPECHECK_SCOPE_HDR_GUARD

#include "Resolver.h"
#include "comma/ast/AstBase.h"
#include "comma/ast/Decl.h"

#include "llvm/ADT/SmallPtrSet.h"

#include <deque>

namespace comma {

class Homonym;

enum ScopeKind {
    DEAD_SCOPE,             ///< Indicates an uninitialized scope.
    BASIC_SCOPE,            ///< Multipurpose scope.
    CUNIT_SCOPE,            ///< Compilation unit scope.
    MODEL_SCOPE,            ///< Signature/domain etc, scope.
    SUBROUTINE_SCOPE,       ///< Subroutine scope.
    RECORD_SCOPE            ///< Record type declaration scope.
};

class Scope {

public:

    // Creates an initial compilation unit scope.
    Scope();

    // On destruction, all frames associated with this scope object are cleared
    // as though thru repeated calls to popScope().
    ~Scope();

    // Returns the kind of this scope.
    ScopeKind getKind() const;

    // Returns the current nesting level, zero based.
    unsigned getLevel() const { return entries.size() - 1; }

    // Returns the number of ScopeEntries currently being managed.
    unsigned numEntries() const { return entries.size(); }

    // Pushes a fresh scope of the given kind.
    void push(ScopeKind kind = BASIC_SCOPE);

    // Moves the scope up one level and unlinks all declarations.
    void pop();

    // Registers the given decl with the current scope.
    //
    // There is only one kind of declaration node which is inadmissible to a
    // Scope, namely DomainInstanceDecls.  For these declarations the
    // corresponding DomoidDecl's are used for lookup.
    //
    // This method looks for any extant direct declarations which conflict with
    // the given decl.  If such a declaration is found, it is returned and the
    // given node is not inserted into the scope.  Otherwise, null is returned
    // and the decl is registered.
    Decl *addDirectDecl(Decl *decl);

    // Adds the given decl to the scope unconditionally.  This method will
    // assert in debug builds if a conflict is found.
    void addDirectDeclNoConflicts(Decl *decl);

    // Adds an import into the scope, making all of the exports from the given
    // type indirectly visible.  Returns true if the given type has already been
    // imported and false otherwise.
    bool addImport(DomainType *type);

    /// Returns a cleared (empty) Resolver object to be used for lookups.
    Resolver &getResolver() {
        resolver.clear();
        return resolver;
    }

    void dump() const;

private:
    // An entry in a Scope object.
    class Entry {

        // The set of lexical declarations associated with this entry.
        typedef llvm::SmallPtrSet<Decl*, 16> DeclSet;

        // Collection of imports associated with this entry.
        typedef llvm::SmallVector<DomainType*, 8> ImportVector;

    public:
        Entry(ScopeKind kind, unsigned tag)
            : kind(kind),
              tag(tag) { }

        // Reinitializes this frame to the specified kind.
        void initialize(ScopeKind kind, unsigned tag) {
            this->kind = kind;
            this->tag  = tag;
        }

        // Returns the kind of this entry.
        ScopeKind getKind() const { return kind; }

        unsigned getTag() const { return tag; }

        void addDirectDecl(Decl *decl);

        void removeDirectDecl(Decl *decl);

        void removeImportDecl(Decl *decl);

        // Returns the number of direct declarations managed by this entry.
        unsigned numDirectDecls() const { return directDecls.size(); }

        // Returns true if this entry contains a direct declaration bound to the
        // given name.
        bool containsDirectDecl(IdentifierInfo *name);

        // Returns true if this the given declaration is directly visible in
        // this entry.
        bool containsDirectDecl(Decl *decl) {
            return directDecls.count(decl);
        }

        void addImportDecl(DomainType *type);

        // Returns the number of imported declarations managed by this frame.
        unsigned numImportDecls() const { return importDecls.size(); }

        bool containsImportDecl(IdentifierInfo *name);
        bool containsImportDecl(DomainType *type);

        // Iterators over the direct declarations managed by this frame.
        typedef DeclSet::const_iterator DirectIterator;
        DirectIterator beginDirectDecls() const { return directDecls.begin(); }
        DirectIterator endDirectDecls()   const { return directDecls.end(); }

        // Iterators over the imports managed by this frame.
        typedef ImportVector::const_iterator ImportIterator;
        ImportIterator beginImportDecls() const { return importDecls.begin(); }
        ImportIterator endImportDecls()  const { return importDecls.end(); }

        // Turns this into an uninitialized (dead) scope entry.  This method is
        // used so that entries can be cached and recognized as inactive
        // objects.
        void clear();

    private:
        ScopeKind    kind;
        unsigned     tag;
        DeclSet      directDecls;
        ImportVector importDecls;

        static Homonym *getOrCreateHomonym(IdentifierInfo *info);

        void importDeclarativeRegion(DeclRegion *region);
        void clearDeclarativeRegion(DeclRegion *region);
    };

    // Type of stack used to maintain our scope frames.
    typedef std::deque<Entry*> EntryStack;
    EntryStack entries;

    // A Resolver instance to facilitate user lookups.
    Resolver resolver;

    // We cache the following number of entries to help ease allocation
    // pressure.
    enum { ENTRY_CACHE_SIZE = 16 };
    Entry *entryCache[ENTRY_CACHE_SIZE];

    // Number of entries currently cached and available for reuse.
    unsigned numCachedEntries;

    // If the given declaration conflicts with another declaration in the
    // current entry, return the declaration with which it conflicts.
    // Otherwise, return null.
    Decl *findConflictingDirectDecl(Decl *decl) const;

    // Returns true if the given declarations conflict.
    bool directDeclsConflict(Decl *X, Decl *Y) const;
};

} // End comma namespace

#endif
