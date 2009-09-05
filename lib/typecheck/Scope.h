//===-- typecheck/Scope.h ------------------------------------- -*- C++ -*-===//
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

#include "llvm/ADT/SmallPtrSet.h"

#include <deque>

namespace comma {

class Homonym;

enum ScopeKind {
    DEAD_SCOPE,             // Indicates an uninitialized scope.
    BASIC_SCOPE,            // multipurpose scope.
    CUNIT_SCOPE,            // compilation unit scope.
    MODEL_SCOPE,            // signature/domain etc, scope.
    FUNCTION_SCOPE          // function scope.
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

    class Resolver {
        typedef llvm::SmallVector<Decl*, 4> DeclVector;
        typedef llvm::SmallVector<ValueDecl*, 4> ValueVector;
        typedef llvm::SmallVector<TypeDecl*, 4> TypeVector;

        // Do not implement.
        Resolver(const Resolver &);
        Resolver &operator=(const Resolver &);

        // Private constructor for use by Scope.
        Resolver() : directDecl(0) { };

        /// Resets this Resolver into its default (empty) state, for use by
        /// Scope.
        void clear();

        friend class Scope;

    public:
        /// Resolves the given identifier and returns true if any bindings were
        /// resolved.
        bool resolve(IdentifierInfo *idInfo);

        /// Filters the overloaded results, keeping only those overloads with
        /// the specified arity.  Returns true if the filter modified the
        /// result.
        bool filterOverloadsWRTArity(unsigned arity);

        /// Filters out all procedure declarations from the results and returns
        /// true if the resolver was modified.
        bool filterProcedures();

        /// Filters out all functional declarations (functions and numeration
        /// literals) from the results and returns true if the resolver was
        /// modified.
        bool filterFunctionals();

        /// Filters out all nullary overloads (procedures and functions of arity
        /// 0, as well as enumeration literals).  Returns true if the resolver
        /// was modified.
        bool filterNullaryOverloads();

        /// Returns the number of direct overloads.
        unsigned numDirectOverloads() const { return directOverloads.size(); }

        /// Returns the number of indirect values.
        unsigned numIndirectValues() const { return indirectValues.size(); }

        /// Returns the number of indirect types.
        unsigned numIndirectTypes() const { return indirectTypes.size(); }

        /// Returns the number of indirect overloads.
        unsigned numIndirectOverloads() const {
            return indirectOverloads.size();
        }

        /// Returns the total number of declarations contained in this resolver.
        unsigned numResolvedDecls() const;

        /// Returns the total number of overloaded declarations.
        unsigned numOverloads() const {
            return numDirectOverloads() + numIndirectOverloads();
        }

        /// Returns true if a direct value has been resolved.
        bool hasDirectValue() const {
            return directDecl && llvm::isa<ValueDecl>(directDecl);
        }

        /// Returns true if a direct type has been resolved.
        bool hasDirectType() const {
            return directDecl && llvm::isa<TypeDecl>(directDecl);
        }

        /// Returns true if a direct capsule has been resolved.
        bool hasDirectCapsule() const {
            return directDecl && llvm::isa<ModelDecl>(directDecl);
        }

        /// Returns true if direct overloads have been resolved.
        bool hasDirectOverloads() const { return numDirectOverloads() != 0; }

        /// Returns true if indirect values have been resolved.
        bool hasIndirectValues() const { return numIndirectValues() != 0; }

        /// Returns true if indirect types have been resolved.
        bool hasIndirectTypes() const { return numIndirectTypes() != 0; }

        /// Returns true if indirect overloads have been resolved.
        bool hasIndirectOverloads() const {
            return numIndirectOverloads() != 0;
        }

        /// Returns the direct value associated with this resolver, or 0 if no
        /// such declaration could be resolved.
        ValueDecl *getDirectValue() const {
            return hasDirectValue() ? llvm::cast<ValueDecl>(directDecl) : 0;
        }

        /// Returns the direct type associated with this resolver, or 0 if no
        /// such declaration could be resolved.
        TypeDecl *getDirectType() const {
            return hasDirectType() ? llvm::cast<TypeDecl>(directDecl) : 0;
        }

        /// Returns the direct capsule associated with this resolver, or 0 if no
        /// such declaration could be resolved.
        ModelDecl *getDirectCapsule() const {
            return hasDirectCapsule() ? llvm::cast<ModelDecl>(directDecl) : 0;
        }

        /// Returns the \p i'th direct overload.  The index must be in range.
        Decl *getDirectOverload(unsigned i) const {
            assert(i < numDirectOverloads() && "Index out of range!");
            return directOverloads[i];
        }

        /// Returns the \p i'th indirect value.  The index must be in range.
        ValueDecl *getIndirectValue(unsigned i) const {
            assert(i < numIndirectValues() && "Index out of range!");
            return indirectValues[i];
        }

        /// Returns the \p i'th indirect type.  The index must be in range.
        TypeDecl *getIndirectType(unsigned i) const {
            assert(i < numIndirectTypes() && "Index out of range!");
            return indirectTypes[i];
        }

        /// Returns the \p i'th indirect overload.  The index must be in range.
        Decl *getIndirectOverload(unsigned i) const {
            assert(i < numIndirectOverloads() && "Index out of range!");
            return indirectOverloads[i];
        }

        /// Returns a vector of all visible subroutine declarations.  Returns
        /// true if any visible subroutines were found.
        bool getVisibleSubroutines(
            llvm::SmallVectorImpl<SubroutineDecl*> &srDecls);

        typedef DeclVector::iterator direct_overload_iter;
        typedef DeclVector::iterator indirect_overload_iter;
        typedef ValueVector::iterator indirect_value_iter;
        typedef TypeVector::iterator indirect_type_iter;

        direct_overload_iter begin_direct_overloads() {
            return directOverloads.begin();
        }
        direct_overload_iter end_direct_overloads()   {
            return directOverloads.end();
        }

        indirect_overload_iter begin_indirect_overloads() {
            return indirectOverloads.begin();
        }
        indirect_overload_iter end_indirect_overloads() {
            return indirectOverloads.end();
        }

        indirect_value_iter begin_indirect_values() {
            return indirectValues.begin();
        }

        indirect_value_iter end_indirect_values() {
            return indirectValues.end();
        }

        indirect_type_iter begin_indirect_types() {
            return indirectTypes.begin();
        }

        indirect_type_iter end_indirect_types() {
            return indirectTypes.end();
        }

    private:
        Decl *directDecl;
        DeclVector directOverloads;
        ValueVector indirectValues;
        TypeVector indirectTypes;
        DeclVector indirectOverloads;

        /// Resolves the direct decls provided by the given homonym.  Returns
        /// true if any decls were resolved.
        bool resolveDirectDecls(Homonym *homonym);

        /// Resolves the indirect decls provided by the given homonym.  Returns
        /// true if any decls were resolved.
        bool resolveIndirectDecls(Homonym *homonym);

        struct ArityPred {
            unsigned arity;
            ArityPred(unsigned arity) : arity(arity) { }
            bool operator()(const Decl* decl) const;
        };

        struct NullaryPred {
            bool operator()(const Decl* decl) const;
        };

        template <typename T>
        struct TypePred {
            bool operator()(const Decl* decl) const {
                return llvm::isa<T>(decl);
            }
        };

        template <typename Pred>
        bool filterOverloads(const Pred &pred) {
            direct_overload_iter directEnd = end_direct_overloads();
            direct_overload_iter directFilterEnd =
                std::remove_if(begin_direct_overloads(), directEnd, pred);

            indirect_overload_iter indirectEnd = end_indirect_overloads();
            indirect_overload_iter indirectFilterEnd =
                std::remove_if(begin_indirect_overloads(), indirectEnd, pred);

            directOverloads.erase(directFilterEnd, directEnd);
            indirectOverloads.erase(indirectFilterEnd, indirectEnd);

            return ((directEnd != directFilterEnd) ||
                    (indirectEnd != indirectFilterEnd));
        }
    };

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
