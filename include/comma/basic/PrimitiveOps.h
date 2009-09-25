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

    EQ_op,
    LT_op,
    GT_op,
    LE_op,
    GE_op,

    ADD_op,
    SUB_op,
    MUL_op,
    POW_op,

    POS_op,
    NEG_op,

    ENUM_op,

    // Tags for fast matching of primitive ID's.
    FIRST_PREDICATE_OP = EQ_op,
    LAST_PREDICATE_OP = GE_op,

    FIRST_PRIMITIVE_OP = EQ_op,
    LAST_PRIMITIVE_OP = NEG_op,

    FIRST_BINARY_OP = EQ_op,
    LAST_BINARY_OP = POW_op,
};

/// Returns true if \p ID names an operator.
inline bool denotesOperator(PrimitiveID ID) {
    return (FIRST_PRIMITIVE_OP <= ID) && (ID <= LAST_PRIMITIVE_OP);
}

/// Returns true if \p ID names a primitive comparison operation.
inline bool denotesPredicateOp(PrimitiveID ID) {
    return (FIRST_PREDICATE_OP <= ID) && (ID <= LAST_PREDICATE_OP);
}

/// Returns true if \p ID names a primitive unary operation.
inline bool denotesUnaryOp(PrimitiveID ID) {
    return ID == POS_op || ID == NEG_op;
}

/// Returns true if \p ID names a primitive binary operation.
inline bool denotesBinaryOp(PrimitiveID ID) {
    return (FIRST_BINARY_OP <= ID) && (ID <= LAST_BINARY_OP);
}

/// Returns the name of a primitive unary operator.
inline const char *getOpName(PrimitiveID ID) {
    switch (ID) {
    default:
        assert(false && "Not a primitive operator!");
        return 0;
    case EQ_op: return "=";
    case LT_op: return "<";
    case GT_op: return ">";
    case LE_op: return "<=";
    case GE_op: return ">=";
    case ADD_op: return "+";
    case SUB_op: return "-";
    case MUL_op: return "*";
    case POW_op: return "**";
    case POS_op: return "+";
    case NEG_op: return "-";
    }
}

} // end primitive_ops namespace.

namespace PO = primitive_ops;

} // end comma namespace.

#endif
