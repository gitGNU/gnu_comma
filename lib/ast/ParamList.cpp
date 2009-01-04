//===-- ast/ParamList.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/ParamList.h"

using namespace comma;

bool ParameterList::hasDuplicateFormals() const
{
    unsigned arity = getArity();

    if (arity == 1 || arity == 0) return false;

    for (unsigned formal = 0; formal < arity - 1; ++formal)
    {
        IdentifierInfo *x = getFormal(formal);
        for (unsigned cursor = formal + 1; cursor < arity; ++cursor)
        {
            IdentifierInfo *y = getFormal(cursor);
            if (x == y) return true;
        }
    }
    return false;
}

bool ParameterList::isDuplicateFormal(const IdentifierInfo *info) const
{
    unsigned index;
    unsigned arity = getArity();

    // Find the given identifier in the parameter list.  If it does not exist,
    // then clearly it does not name a duplicate.
    for (index = 0; index < arity; ++index)
        if (info == getFormal(index))
            break;
    if (index == arity) return false;

    // Locate a matching identifier preceeding this one.
    for (unsigned i = 0; i < index; ++i)
    {
        IdentifierInfo *x = getFormal(i);
        if (x == info)
            return true;;
    }
    return false;
}
