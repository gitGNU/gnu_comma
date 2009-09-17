//===-- typecheck/Resolver.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_RESOLVER_HDR_GUARD
#define COMMA_TYPECHECK_RESOLVER_HDR_GUARD

#include "comma/ast/Decl.h"

namespace comma {

class Homonym;
class Scope;

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

    /// Returns the IdentifierInfo this resolver is associated with.
    IdentifierInfo *getIdInfo() const { return idInfo; }

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

    /// Returns true if there is a unique visible indirect value.
    bool hasVisibleIndirectValue() const {
        return (numIndirectValues() == 1 &&
                (!hasIndirectOverloads() && !hasIndirectTypes()));
    }

    /// Returns true if there is a unique visible indirect type.
    bool hasVisibleIndirectType() const {
        return (numIndirectTypes() == 1 &&
                (!hasIndirectValues() && !hasIndirectOverloads()));
    }

    /// Returns true if there are visible indirect overloads.
    bool hasVisibleIndirectOverloads() const {
        return (hasIndirectOverloads() &&
                (!hasIndirectValues() && !hasIndirectTypes()));
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
    IdentifierInfo *idInfo;
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



} // end comma namespace.

#endif
