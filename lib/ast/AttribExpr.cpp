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
        ID = attrib::First;
        break;

    case AST_LastAE:
        ID = attrib::Last;
        break;
    };

    return ID;
}
