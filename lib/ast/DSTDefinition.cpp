//===-- ast/DSTDefinition.cpp --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/DSTDefinition.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Type.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DSTDefinition::DSTDefinition(Location loc, Ast *base, DSTTag tag)
    : Ast(AST_DSTDefinition),
      loc(loc),
      definition(base)
{
    bits = tag;

    // Ensure the given tag and AST node are compatible.
    //
    // FIXME: Wrap in a DEBUG macro or similar.
    switch (tag) {
    case Attribute_DST:
        assert(isa<RangeAttrib>(base) && "Inconsistent ast/tag combo!");
        break;

    case Range_DST:
    case Type_DST:
    case Constrained_DST:
    case Unconstrained_DST:
        assert(isa<DiscreteType>(base) && "Inconsistent ast/tag combo!");
        break;
    }
}

DiscreteType *DSTDefinition::getType()
{
    DiscreteType *result = 0;

    switch (getTag()) {
    case Attribute_DST:
        result = cast<RangeAttrib>(definition)->getType();
        break;
    case Range_DST:
    case Type_DST:
    case Constrained_DST:
    case Unconstrained_DST:
        result = cast<DiscreteType>(definition);
        break;
    }
    return result;
}

const Range *DSTDefinition::getRange() const
{
    if (definedUsingRange())
        return cast<DiscreteType>(definition)->getConstraint();
    return 0;
}

Range *DSTDefinition::getRange()
{
    if (definedUsingRange())
        return cast<DiscreteType>(definition)->getConstraint();
    return 0;
}

const RangeAttrib *DSTDefinition::getAttrib() const
{
    return dyn_cast<RangeAttrib>(definition);
}

RangeAttrib *DSTDefinition::getAttrib()
{
    return dyn_cast<RangeAttrib>(definition);
}
