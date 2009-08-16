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
    EnumFunction
};

} // end primitive_ops namespace.

namespace PO = primitive_ops;

} // end comma namespace.

#endif
