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

    //
    // Binary arithmetic operators.
    //
    ADD_op,
    SUB_op,
    MUL_op,
    DIV_op,
    MOD_op,
    REM_op,
    POW_op,

    //
    // Binary predicates.
    //
    EQ_op,
    NE_op,
    LT_op,
    GT_op,
    LE_op,
    GE_op,
    LOR_op,
    LAND_op,
    LXOR_op,

    //
    // Unary predicates.
    //
    LNOT_op,

    //
    // Unary arithmetic operators.
    POS_op,
    NEG_op,

    //
    // Miscellaneous operators.
    //
    ENUM_op,

    // Tags for fast matching of primitive ID's.
    FIRST_PREDICATE_OP = EQ_op,
    LAST_PREDICATE_OP = LNOT_op,

    FIRST_PRIMITIVE_OP = ADD_op,
    LAST_PRIMITIVE_OP = ENUM_op,

    FIRST_BINARY_OP = ADD_op,
    LAST_BINARY_OP = LXOR_op,

    FIRST_UNARY_OP = LNOT_op,
    LAST_UNARY_OP = NEG_op
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
    return (FIRST_UNARY_OP <= ID) && (ID <= LAST_UNARY_OP);
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
    case NE_op: return "/=";
    case LT_op: return "<";
    case GT_op: return ">";
    case LE_op: return "<=";
    case GE_op: return ">=";
    case ADD_op: return "+";
    case SUB_op: return "-";
    case MUL_op: return "*";
    case DIV_op: return "/";
    case MOD_op: return "mod";
    case REM_op: return "rem";
    case POW_op: return "**";
    case POS_op: return "+";
    case NEG_op: return "-";
    case LOR_op: return "or";
    case LAND_op: return "and";
    case LNOT_op: return "not";
    case LXOR_op: return "xor";
    }
}

} // end primitive_ops namespace.

namespace PO = primitive_ops;

} // end comma namespace.

#endif
