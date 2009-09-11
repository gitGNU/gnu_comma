//===-- basic/PrimitiveOps.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_PRIMITIVEOPS_HDR_GUARD
#define COMMA_BASIC_PRIMITIVEOPS_HDR_GUARD

namespace comma {

namespace primitive_ops {

enum PrimitiveID {

    NotPrimitive,

    Equality,
    LessThan,
    GreaterThan,
    LessThanOrEqual,
    GreaterThanOrEqual,
    Plus,
    Minus,
    Pos,
    Neg,
    EnumFunction
};

/// Returns true if \p ID names a primitive unary operation.
inline bool denotesUnaryPrimitive(PrimitiveID ID) {
    return ID == Pos || ID == Neg;
}

/// Returns true if \p ID names a primitive binary operation.
inline bool denotesBinaryPrimitive(PrimitiveID ID) {
    return !denotesUnaryPrimitive(ID);
}

} // end primitive_ops namespace.

namespace PO = primitive_ops;

} // end comma namespace.

#endif
