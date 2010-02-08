//===-- ast/AttribExpr.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AttribExpr.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Evaluates \p expr as a static discrete expression.  Returns the value of the
/// expression as a zero-based unsigned quantity.  This function asserts that \p
/// expr is a static discrete expression, and that its value is strictly
/// positive and does not exceed the rank of \p arrTy.
unsigned checkArrayDimension(ArrayType *arrTy, Expr *expr)
{
    llvm::APInt dim;
    bool isStatic = expr->staticDiscreteValue(dim);

    assert(isStatic && "Length dimension not static!");
    ((void)isStatic);

    assert(dim.getMinSignedBits() <= sizeof(unsigned)*8 &&
           "Cannot represent dimension!");

    unsigned result = unsigned(dim.getSExtValue());
    assert(result > 0 && "Non-positive dimension!");
    assert(result <= arrTy->getRank() && "Dimension too large!");

    return --result;
}

} // end anonymous namespace.

attrib::AttributeID AttribExpr::correspondingID(AstKind kind)
{
    attrib::AttributeID ID;

    switch (kind) {

    default:
        ID = attrib::UNKNOWN_ATTRIBUTE;
        break;

    case AST_FirstAE:
    case AST_FirstArrayAE:
        ID = attrib::First;
        break;

    case AST_LastAE:
    case AST_LastArrayAE:
        ID = attrib::Last;
        break;

    case AST_LengthAE:
        ID = attrib::Length;
        break;
    };

    return ID;
}

//===----------------------------------------------------------------------===//
// ArrayBoundAE

ArrayBoundAE::ArrayBoundAE(AstKind kind,
                           Expr *prefix, Expr *dimension, Location loc)
    : AttribExpr(kind, prefix, loc),
      dimExpr(dimension)
{
    assert(kind == AST_FirstArrayAE || kind == AST_LastArrayAE);
    ArrayType *arrTy = cast<ArrayType>(prefix->getType());
    dimValue = checkArrayDimension(arrTy, dimension);
    setType(arrTy->getIndexType(dimValue));
}

ArrayBoundAE::ArrayBoundAE(AstKind kind, Expr *prefix, Location loc)
    : AttribExpr(kind, prefix, loc),
      dimExpr(0),
      dimValue(0)
{
    assert(kind == AST_FirstArrayAE || kind == AST_LastArrayAE);
    ArrayType *arrTy = cast<ArrayType>(prefix->getType());
    setType(arrTy->getIndexType(0));
}

//===----------------------------------------------------------------------===//
// LengthAE

LengthAE::LengthAE(Expr *prefix, Location loc, Expr *dimension)
    : AttribExpr(AST_LengthAE, prefix, UniversalType::getUniversalInteger(),
                 loc),
      dimExpr(dimension)
{
    ArrayType *arrTy = cast<ArrayType>(prefix->getType());
    dimValue = dimExpr ? checkArrayDimension(arrTy, dimension) : 0;
}
