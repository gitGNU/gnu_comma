//===-- typecheck/Eval.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Implementation of the compile-time expression evaluation routines.
//===----------------------------------------------------------------------===//

#include "Eval.h"
#include "comma/ast/Expr.h"

#include "llvm/ADT/APInt.h"

using namespace comma;
using namespace comma::eval;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Attempts to evaluate a static integer-valued function call.
///
/// \param expr A function call expression.
///
/// \param If \p expr was successfully evaluated, \p result is set to the
/// computed value.
///
/// \return True if \p expr was static and \p result was set. False otherwise.
bool staticIntegerFunctionValue(FunctionCallExpr *expr, llvm::APInt &result);

/// Attempts to evaluate a static, unary, integer-valued function call.
///
/// \param ID The primitive unary operation to evaluate.
///
/// \param arg An expression.
///
/// \param If \p arg was successfully evaluated, \p result is set to the
/// computed value.
///
/// \return True if \p arg was static and \p result was set.  False otherwise.
bool staticIntegerUnaryValue(PO::PrimitiveID ID,
                             Expr *expr, llvm::APInt &result);

/// Attempts to evaluate a static, binary, integer-valued function call.
///
/// \param ID The primitive binary operation to evaluate.
///
/// \param x The left hand side of the binary operation.
///
/// \param y The right hand side of the binary operation.
///
/// \param If \p x and \p y were successfully evaluated, \p result is set to the
/// computed value.
///
/// \return True if the evaluation was successful.
bool staticIntegerBinaryValue(PO::PrimitiveID ID, Expr *x, Expr *y,
                              llvm::APInt &result);

PO::PrimitiveID getCallPrimitive(FunctionCallExpr *call)
{
    if (call->isAmbiguous())
        return PO::NotPrimitive;
    else {
        FunctionDecl *decl = cast<FunctionDecl>(call->getConnective());
        return decl->getPrimitiveID();
    }
}

void signExtend(llvm::APInt &x, llvm::APInt &y);

/// Zero extends the given APInt by one bit.
inline llvm::APInt &zeroExtend(llvm::APInt &x) {
    x.zext(x.getBitWidth() + 1);
    return x;
}

/// Negates \p x without overflow.  May extend the bit width of \p x.
inline llvm::APInt &negate(llvm::APInt &x) {
    if (x.isMinSignedValue())
        zeroExtend(x);
    else {
        x.flip();
        ++x;
    }
    return x;
}

/// Minimizes the number of bits required to represent the given APInt.
inline llvm::APInt &minimizeWidth(llvm::APInt &x)
{
    return x.trunc(x.getMinSignedBits());
}

/// Basic arithmetic operations which perform bit extensions on an as-needed
/// basis.
llvm::APInt add(llvm::APInt x, llvm::APInt y);
llvm::APInt subtract(llvm::APInt x, llvm::APInt y);
llvm::APInt multiply(llvm::APInt x, llvm::APInt y);
llvm::APInt exponentiate(llvm::APInt x, llvm::APInt y);

//===----------------------------------------------------------------------===//
// Implementations.

bool staticIntegerFunctionValue(FunctionCallExpr *expr, llvm::APInt &result)
{
    PO::PrimitiveID ID = getCallPrimitive(expr);

    if (ID == PO::NotPrimitive)
        return false;

    typedef FunctionCallExpr::arg_iterator iterator;
    iterator I = expr->begin_arguments();
    if (PO::denotesUnaryOp(ID)) {
        assert(expr->getNumArgs() == 1);
        Expr *arg = *I;
        return staticIntegerUnaryValue(ID, arg, result);
    }
    else {
        assert(expr->getNumArgs() == 2);
        Expr *lhs = *I;
        Expr *rhs = *(++I);
        return staticIntegerBinaryValue(ID, lhs, rhs, result);
    }
}

bool staticIntegerBinaryValue(PO::PrimitiveID ID, Expr *x, Expr *y,
                              llvm::APInt &result)
{
    llvm::APInt LHS, RHS;
    if (!staticIntegerValue(x, LHS) || !staticIntegerValue(y, RHS))
        return false;

    switch (ID) {

    default:
        return false;

    case PO::ADD_op:
        result = add(LHS, RHS);
        break;

    case PO::SUB_op:
        result = subtract(LHS, RHS);
        break;

    case PO::MUL_op:
        result = multiply(LHS, RHS);
        break;

    case PO::POW_op:
        result = exponentiate(LHS, RHS);
        break;
    }
    return true;
}

bool staticIntegerUnaryValue(PO::PrimitiveID ID, Expr *arg, llvm::APInt &result)
{
    if (!staticIntegerValue(arg, result))
        return false;

    // There are only two unary operations to consider.  Negation and the
    // "Pos" operation (which does nothing).
    switch (ID) {
    default:
        assert(false && "Bad primitive ID for a unary operator!");
        return false;
    case PO::NEG_op:
        negate(result);
        break;
    case PO::POS_op:
        break;
    }
    return true;
}

void signExtend(llvm::APInt &x, llvm::APInt &y)
{
    unsigned xWidth = x.getBitWidth();
    unsigned yWidth = y.getBitWidth();
    unsigned target = std::max(xWidth, yWidth);

    if (xWidth < yWidth)
        x.sext(target);
    else if (yWidth < xWidth)
        y.sext(target);
}

llvm::APInt add(llvm::APInt x, llvm::APInt y)
{
    if (y.isNonNegative()) {
        signExtend(x, y);
        llvm::APInt result(x + y);

        // If the addition overflows, zero extend the result.
        if (result.slt(x))
            zeroExtend(result);
        return result;
    }
    else {
        signExtend(x, negate(y));
        llvm::APInt result(x - y);

        // If the subtraction overflows, zero extend the result.
        if (result.sgt(x))
            zeroExtend(result);
        return result;
    }
}

llvm::APInt subtract(llvm::APInt x, llvm::APInt y)
{
    return add(x, negate(y));
}

llvm::APInt multiply(llvm::APInt x, llvm::APInt y)
{
    unsigned xWidth = x.getBitWidth();
    unsigned yWidth = y.getBitWidth();
    unsigned target = 2 * std::max(xWidth, yWidth);
    x.sext(target);
    y.sext(target);
    llvm::APInt result(x * y);
    return minimizeWidth(result);
}

llvm::APInt exponentiate(llvm::APInt x, llvm::APInt y)
{
    assert(y.isNonNegative() && "Negative power in exponentiation!");

    if (y == 0) {
        x = 1;
        return minimizeWidth(x);
    }

    llvm::APInt result(x);
    while (--y != 0)
        result = multiply(result, x);
    return result;
}

} // end anonymous namespace.

bool eval::staticIntegerValue(Expr *expr, llvm::APInt &result)
{
    if (IntegerLiteral *ILit = dyn_cast<IntegerLiteral>(expr)) {
        result = ILit->getValue();
        return true;
    }

    if (FunctionCallExpr *FCall = dyn_cast<FunctionCallExpr>(expr))
        return staticIntegerFunctionValue(FCall, result);

    if (ConversionExpr *CExpr = dyn_cast<ConversionExpr>(expr)) {
        // FIXME:  More work will be needed here.
        return staticIntegerValue(CExpr->getOperand(), result);
    }

    return false;
}
