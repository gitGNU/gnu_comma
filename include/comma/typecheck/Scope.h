//===-- tyecheck/Scope.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_SCOPE_HDR_GUARD
#define COMMA_TYPECHECK_SCOPE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Homonym.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <deque>

namespace comma {

class Scope;

enum ScopeKind {
    DEAD_SCOPE,             // Indicates an uninitialized scope.
    BASIC_SCOPE,            // multipurpose scope.
    CUNIT_SCOPE,            // compilation unit scope.
    MODEL_SCOPE,            // signature/domain etc, scope.
    FUNCTION_SCOPE          // function scope.
};

// An entry in a Scope object.
class ScopeEntry {

    // The set of lexical declarations associated with this entry.
    typedef llvm::SmallPtrSet<Decl*, 16> DeclSet;

    // Collection of imports associated with this entry.
    typedef llvm::SmallVector<DomainType*, 8> ImportVector;

public:
    ScopeEntry(ScopeKind kind, unsigned tag)
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

    // Returns the number of direct declarations managed by this entry.
    unsigned numDirectDecls() const { return directDecls.size(); }

    // Returns true if this entry contains a direct declaration bound to the
    // given name.
    bool containsDirectDecl(IdentifierInfo *name);

    // Returns true if this teh given declaration is directly visible in this
    // entry.
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
    // used so that entries can be cached and recognized as inactive objects.
    void clear();

private:
    ScopeKind    kind;
    unsigned     tag;
    DeclSet      directDecls;
    ImportVector importDecls;

    static Homonym *getOrCreateHomonym(IdentifierInfo *info);
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
    unsigned getLevel() const;

    // Returns the number of ScopeEntries currently being managed.
    unsigned numEntries() const { return entries.size(); }

    // Returns the current active entry in this scope.
    ScopeEntry *currentEntry() const { return entries.front(); }

    // Pushes a fresh scope of the given kind.
    void push(ScopeKind kind = BASIC_SCOPE);

    // Moves the scope up one level and unlinks all declarations.
    void pop();

    void addDirectDecl(Decl *decl) {
        entries.front()->addDirectDecl(decl);
    }

    void addDirectModel(ModelDecl *model) {
        entries.front()->addDirectDecl(model);
    }

    void addDirectValue(ValueDecl *value) {
        entries.front()->addDirectDecl(value);
    }

    void addDirectSubroutine(SubroutineDecl *routine) {
        entries.front()->addDirectDecl(routine);
    }

    // Adds an import into the scope, making all of the exports from the given
    // type visible.  Returns true if the given type has already been imported
    // and false otherwise.
    bool addImport(DomainType *type);

    ModelDecl *lookupDirectModel(const IdentifierInfo *name,
                                 bool traverse = true) const;

    ValueDecl *lookupDirectValue(const IdentifierInfo *name) const;

    void dump() const;

private:
    // Type of stack used to maintain our scope frames.
    typedef std::deque<ScopeEntry*> EntryStack;
    EntryStack entries;

    // We cache the following number of entries to help ease allocation
    // pressure.
    enum { ENTRY_CACHE_SIZE = 16 };
    ScopeEntry *entryCache[ENTRY_CACHE_SIZE];

    // Number of entries currently cached and available for reuse.
    unsigned numCachedEntries;
};

} // End comma namespace

#endif
