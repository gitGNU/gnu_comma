//===-- ast/AttribExpr.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AttribExpr.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

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
    assert(dimension->isStaticDiscreteExpr());

    ArrayType *arrTy = cast<ArrayType>(prefix->getType());

    llvm::APInt dim;
    dimension->staticDiscreteValue(dim);
    assert(dim.getMinSignedBits() <= sizeof(unsigned)*8 &&
           "Cannot represent dimension!");

    dimValue = unsigned(dim.getSExtValue());
    assert(dimValue > 0 && "Non-positive dimension!");
    assert(dimValue <= arrTy->getRank() && "Dimension too large!");

    // Make the dimension value zero based and set the type to correspond to the
    // index type of the array.
    --dimValue;
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
