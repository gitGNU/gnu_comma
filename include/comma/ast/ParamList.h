//===-- ast/ParamList.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_PARAMLIST_HDR_GUARD
#define COMMA_AST_PARAMLIST_HDR_GUARD

#include "comma/ast/AstBase.h"
#include <utility>
#include <vector>

namespace comma {

// A ParameterList class represents the formal parameters of signatures,
// domains, and functions.
class ParameterList {

public:
    typedef std::pair<IdentifierInfo *, Type *> ParameterEntry;
    typedef std::vector<ParameterEntry> ParameterSet;

    // Creates an empty parameter list.
    ParameterList() { }

    // Adds a parameter to this parameter list.
    void addParameter(IdentifierInfo *formal, Type *type) {
        params.push_back(ParameterEntry(formal, type));
    }

    // Returns the number of parameters in this list.
    unsigned getArity() const { return params.size(); }

    // Returns the n'th identifier.
    IdentifierInfo *getFormal(unsigned n) const {
        assert(n < params.size());
        return params[n].first;
    }

    // Returns the n'th type.
    Type *getType(unsigned n) const {
        assert(n < params.size());
        return params[n].second;
    }

    // Iterators over the parameters: supplies objects of type
    // std::pair<Identifier*, Ast*> corresponding to each parameter of this
    // list.
    typedef ParameterSet::const_iterator iterator;
    iterator begin() const { return params.begin(); }
    iterator end()   const { return params.end(); }

    // Returns true if any formal parameter names are duplicates.
    bool hasDuplicateFormals() const;

    // Returns true if the given identifier duplicates a previous parameter. The
    // given identifier need not be a member of the parameter list (in which
    // case this function returns false).
    bool isDuplicateFormal(const IdentifierInfo *identifier) const;

private:
    ParameterSet params;
};

} // End comma namespace

#endif
