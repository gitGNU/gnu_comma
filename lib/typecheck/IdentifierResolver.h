//===-- typecheck/IdentifierResolver.h ------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_IDENTIFIERRESOLVER_HDR_GUARD
#define COMMA_TYPECHECK_IDENTIFIERRESOLVER_HDR_GUARD

#include "comma/ast/Homonym.h"
#include "llvm/ADT/SmallVector.h"

namespace comma {

class IdentifierResolver {

    typedef llvm::SmallVector<Decl*, 4>      DeclVector;
    typedef llvm::SmallVector<ValueDecl*, 4> ValueVector;

    // Do not implement.
    IdentifierResolver(const IdentifierResolver &);
    IdentifierResolver &operator=(const IdentifierResolver &);

public:
    IdentifierResolver() : directValue(0) { }

    /// Resolves the given identifier and returns true if any bindings were
    /// resolved.
    bool resolve(IdentifierInfo *idInfo);

    /// Resets this IdentifierResolver into its default state.
    void clear();

    /// Filters the overloaded results, keeping only those overloads with the
    /// specified arity.  Returns true if the filter modified the result.
    bool filterOverloadsWRTArity(unsigned arity);

    /// Filters out all procedure declarations from the results and returns true
    /// if the resolver was modified.
    bool filterProcedures();

    /// Filters out all functional declarations (functions and numeration
    /// literals) from the results and returns true if the resolver was
    /// modified.
    bool filterFunctionals();

    /// Filters out all nullary overloads (procedures and functions of arity 0,
    /// as well as enumeration literals).  Returns true if the resolver was
    /// modified.
    bool filterNullaryOverloads();

    /// Returns the number of direct overloads.
    unsigned numDirectOverloads() const { return directOverloads.size(); }

    /// Returns the number of indirect values.
    unsigned numIndirectValues() const { return indirectValues.size(); }

    /// Returns the number of indirect overloads.
    unsigned numIndirectOverloads() const { return indirectOverloads.size(); }

    /// Returns the total number of declarations contained in this resolver.
    unsigned numResolvedDecls() const;

    /// Returns the total number of overloaded declarations.
    unsigned numOverloads() const {
        return numDirectOverloads() + numIndirectOverloads();
    }

    /// Returns true if a direct value has been resolved.
    bool hasDirectValue() const { return directValue != 0; }

    /// Returns true if direct overloads have been resolved.
    bool hasDirectOverloads() const { return numDirectOverloads() != 0; }

    /// Returns true if indirect values have been resolved.
    bool hasIndirectValues() const { return numIndirectValues() != 0; }

    /// Returns true if indirect overloads have been resolved.
    bool hasIndirectOverloads() const { return numIndirectOverloads() != 0; }

    /// Returns the direct value associated with this resolver, or 0 if no
    /// such declaration could be resolved.
    ValueDecl *getDirectValue() const { return directValue; }

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

    /// Returns the \p i'th indirect overload.  The index must be in range.
    Decl *getIndirectOverload(unsigned i) const {
        assert(i < numIndirectOverloads() && "Index out of range!");
        return indirectOverloads[i];
    }

    typedef DeclVector::iterator  DirectOverloadIter;
    typedef DeclVector::iterator  IndirectOverloadIter;
    typedef ValueVector::iterator IndirectValueIter;

    DirectOverloadIter beginDirectOverloads() {
        return directOverloads.begin();
    }
    DirectOverloadIter endDirectOverloads()   {
        return directOverloads.end();
    }

    IndirectOverloadIter beginIndirectOverloads() {
        return indirectOverloads.begin();
    }
    IndirectOverloadIter endIndirectOverloads() {
        return indirectOverloads.end();
    }

    IndirectValueIter beginIndirectValues() {
        return indirectValues.begin();
    }

    IndirectValueIter endIndirectValues() {
        return indirectValues.end();
    }

private:
    ValueDecl  *directValue;
    DeclVector  directOverloads;
    ValueVector indirectValues;
    DeclVector  indirectOverloads;

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
    bool filterOverloads(const Pred &pred);
};

template <typename Pred>
bool IdentifierResolver::filterOverloads(const Pred &pred)
{
    DirectOverloadIter directEnd =
        std::remove_if(beginDirectOverloads(), endDirectOverloads(), pred);

    IndirectOverloadIter indirectEnd =
        std::remove_if(beginIndirectOverloads(), endIndirectOverloads(), pred);

    bool result = ((directEnd != endDirectOverloads()) ||
                   (indirectEnd != endIndirectOverloads()));

    directOverloads.erase(directEnd, endDirectOverloads());
    indirectOverloads.erase(indirectEnd, endIndirectOverloads());

    return result;
}

} // End comma namespace.

#endif
