//===-- ast/Eval.cpp ------------------------------------------ -*- C++ -*-===//
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

#include "comma/ast/AttribExpr.h"
#include "comma/ast/Expr.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Attempts to evaluate a static discrete valued function call.
///
/// \param expr A function call expression.
///
/// \param result If \p expr was successfully evaluated, \p result is set to the
/// computed value.
///
/// \return True if \p expr was static and \p result was set. False otherwise.
bool staticDiscreteFunctionValue(const FunctionCallExpr *expr,
                                 llvm::APInt &result);

/// Attempts to evaluate a static, unary, discrete valued function call.
///
/// \param ID The primitive unary operation to evaluate.
///
/// \param arg An expression.
///
/// \param result If \p arg was successfully evaluated, \p result is set to the
/// computed value.
///
/// \return True if \p arg was static and \p result was set.  False otherwise.
bool staticDiscreteUnaryValue(PO::PrimitiveID ID,
                             const Expr *expr, llvm::APInt &result);

/// Attempts to evaluate a static, binary, discrete valued function call.
///
/// \param ID The primitive binary operation to evaluate.
///
/// \param x The left hand side of the binary operation.
///
/// \param y The right hand side of the binary operation.
///
/// \param result If \p x and \p y were successfully evaluated, \p result is set
/// to the computed value.
///
/// \return True if the evaluation was successful.
bool staticDiscreteBinaryValue(PO::PrimitiveID ID,
                               const Expr *x, const Expr *y,
                               llvm::APInt &result);

/// Attempts to evaluate a static discrete valued attribute expression.
///
/// \param expr The attribute to evaluate.
///
/// \param result If \p expr was successfully exvaluated, \p result is set to
/// the computed value.
bool staticDiscreteAttribExpr(const AttribExpr *expr, llvm::APInt &result);

PO::PrimitiveID getCallPrimitive(const FunctionCallExpr *call)
{
    if (call->isAmbiguous())
        return PO::NotPrimitive;
    else {
        const FunctionDecl *decl = cast<FunctionDecl>(call->getConnective());
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

bool staticDiscreteFunctionValue(const FunctionCallExpr *expr,
                                 llvm::APInt &result)
{
    PO::PrimitiveID ID = getCallPrimitive(expr);

    if (ID == PO::NotPrimitive)
        return false;

    typedef FunctionCallExpr::const_arg_iterator iterator;
    iterator I = expr->begin_arguments();
    if (PO::denotesUnaryOp(ID)) {
        assert(expr->getNumArgs() == 1);
        const Expr *arg = *I;
        return staticDiscreteUnaryValue(ID, arg, result);
    }
    else if (PO::denotesBinaryOp(ID)) {
        assert(expr->getNumArgs() == 2);
        const Expr *lhs = *I;
        const Expr *rhs = *(++I);
        return staticDiscreteBinaryValue(ID, lhs, rhs, result);
    }
    else if (ID == PO::ENUM_op) {
        const EnumLiteral *lit = cast<EnumLiteral>(expr->getConnective());
        const EnumerationType *enumTy = lit->getReturnType();
        unsigned idx = lit->getIndex();
        unsigned size = enumTy->getSize();
        result = llvm::APInt(size, idx);
        return true;
    }
    else
        // All other primitives do not denote integer valued expressions.
        return false;
}

bool staticDiscreteBinaryValue(PO::PrimitiveID ID,
                               const Expr *x, const Expr *y,
                               llvm::APInt &result)
{
    llvm::APInt LHS, RHS;
    if (!x->staticDiscreteValue(LHS) || !y->staticDiscreteValue(RHS))
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

bool staticDiscreteUnaryValue(PO::PrimitiveID ID, const Expr *arg,
                              llvm::APInt &result)
{
    if (!arg->staticDiscreteValue(result))
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

bool staticDiscreteAttribExpr(const AttribExpr *expr, llvm::APInt &result)
{
    bool status = false;

    // Current attribute support is minimal.  Only First and Last are currently
    // supported.
    switch (expr->getKind()) {

    default:
        // The given attribute cannot be evaluated statically.
        break;

    case Ast::AST_FirstAE: {
        const IntegerType *intTy = cast<FirstAE>(expr)->getType();
        if (const RangeConstraint *constraint = intTy->getConstraint()) {
            if (constraint->hasStaticLowerBound()) {
                result = constraint->getStaticLowerBound();
                status = true;
            }
        }
        else if (intTy->isRootType()) {
            intTy->getLowerLimit(result);
            status = true;
        }
        break;
    }

    case Ast::AST_LastAE: {
        const IntegerType *intTy = cast<LastAE>(expr)->getType();
        if (const RangeConstraint *constraint = intTy->getConstraint()) {
            if (constraint->hasStaticUpperBound()) {
                result = constraint->getStaticUpperBound();
                status = true;
            }
        }
        else if (intTy->isRootType()) {
            intTy->getUpperLimit(result);
            status = true;
        }
        break;
    }
    };
    return status;
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

bool Expr::staticDiscreteValue(llvm::APInt &result) const
{
    if (const IntegerLiteral *ILit = dyn_cast<IntegerLiteral>(this)) {
        result = ILit->getValue();
        return true;
    }

    if (const FunctionCallExpr *FCall = dyn_cast<FunctionCallExpr>(this))
        return staticDiscreteFunctionValue(FCall, result);

    if (const ConversionExpr *CExpr = dyn_cast<ConversionExpr>(this))
        return CExpr->getOperand()->staticDiscreteValue(result);

    if (const AttribExpr *AExpr = dyn_cast<AttribExpr>(this))
        return staticDiscreteAttribExpr(AExpr, result);

    return false;
}

bool Expr::isStaticDiscreteExpr() const
{
    llvm::APInt tmp;
    return staticDiscreteValue(tmp);
}

bool Expr::staticStringValue(std::string &result) const
{
    // The only static string values ATM are string literals.
    if (const StringLiteral *lit = dyn_cast<StringLiteral>(this)) {
        result = lit->getString().str();
        return true;
    }
    return false;
}

bool Expr::isStaticStringExpr() const
{
    return isa<StringLiteral>(this);
}

