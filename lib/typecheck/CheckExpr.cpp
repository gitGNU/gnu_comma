//===-- typecheck/CheckExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Qualifier.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"
#include "comma/typecheck/TypeCheck.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

bool TypeCheck::checkApplicableArgument(Expr *arg, Type *targetType)
{
    // FIXME: This is a hack.  The type equality predicates should perform this
    // reduction.
    if (CarrierType *carrierTy = dyn_cast<CarrierType>(targetType))
        targetType = carrierTy->getRepresentationType();

    // If the argument as a fully resolved type, all we currently do is test for
    // type equality.
    if (arg->hasType()) {
        Type *argTy = arg->getType();

        // FIXME: This is a hack.  The type equality predicates should perform
        // this reduction.
        if (CarrierType *carrierTy = dyn_cast<CarrierType>(argTy))
            argTy = carrierTy->getRepresentationType();

        if (!targetType->equals(argTy))
            return false;
        else
            return true;
    }

    // We have an unresolved argument expression.  If the expression is an
    // integer literal it is compatable if the target is an integer type.
    //
    // FIXME:  We should also check that the literal satisfies range contraints
    // of the target.
    if (isa<IntegerLiteral>(arg))
        return targetType->isIntegerType();

    // The expression must be an ambiguous function call.  Check that at least
    // one interpretation of the call satisfies the target type.
    typedef FunctionCallExpr::connective_iterator iterator;
    bool applicableArgument = false;
    FunctionCallExpr *call = cast<FunctionCallExpr>(arg);
    iterator I = call->begin_connectives();
    iterator E = call->end_connectives();

    for ( ; I != E; ++I) {
        FunctionDecl *connective = *I;
        Type *returnType = connective->getReturnType();
        if (targetType->equals(returnType)) {
            applicableArgument = true;
            break;
        }
    }

    return applicableArgument;
}

bool TypeCheck::routineAcceptsKeywords(SubroutineDecl *decl,
                                       unsigned numPositional,
                                       SVImpl<KeywordSelector*>::Type &keys)
{
    for (unsigned j = 0; j < keys.size(); ++j) {
        KeywordSelector *selector = keys[j];
        IdentifierInfo *key = selector->getKeyword();
        int keyIndex = decl->getKeywordIndex(key);

        if (keyIndex < 0 || unsigned(keyIndex) < numPositional)
            return false;
    }
    return true;
}

/// Checks that the given subroutine decl accepts the provided positional
/// arguments.
bool TypeCheck::routineAcceptsArgs(SubroutineDecl *decl,
                                   SVImpl<Expr*>::Type &args)
{
    unsigned numArgs = args.size();
    for (unsigned i = 0; i < numArgs; ++i) {
        Expr *arg = args[i];
        Type *targetType = decl->getParamType(i);

        if (!checkApplicableArgument(arg, targetType))
            return false;
    }
    return true;
}

/// Checks that the given subroutine decl accepts the provided keyword
/// arguments.
bool
TypeCheck::routineAcceptsArgs(SubroutineDecl *decl,
                              SVImpl<KeywordSelector*>::Type &args)
{
    unsigned numKeys = args.size();
    for (unsigned i = 0; i < numKeys; ++i) {
        KeywordSelector *selector = args[i];
        Expr *arg = selector->getExpression();
        IdentifierInfo *key = selector->getKeyword();
        unsigned targetIndex = decl->getKeywordIndex(key);
        Type *targetType = decl->getParamType(targetIndex);

        if (!checkApplicableArgument(arg, targetType))
            return false;
    }
    return true;
}

Ast *TypeCheck::acceptSubroutineCall(SubroutineRef *ref,
                                     SVImpl<Expr*>::Type &positionalArgs,
                                     SVImpl<KeywordSelector*>::Type &keyedArgs)
{
    Location loc = ref->getLocation();
    unsigned numPositional = positionalArgs.size();
    unsigned numKeys = keyedArgs.size();

    if (ref->isResolved())
        return checkSubroutineCall(ref, positionalArgs, keyedArgs);

    // Reduce the subroutine reference to include only those which can accept
    // the keyword selectors provided.
    SubroutineRef::iterator I = ref->begin();
    while (I != ref->end()) {
        SubroutineDecl *decl = *I;
        if (routineAcceptsKeywords(decl, numPositional, keyedArgs))
            ++I;
        else
            I = ref->erase(I);
    }

    // If none of the declarations support the keywords given, just report the
    // call as ambiguous.
    if (ref->empty()) {
        report(loc, diag::AMBIGUOUS_EXPRESSION);
        return 0;
    }

    // Reduce the set of declarations with respect to the types of the
    // arguments.
    for (I = ref->begin(); I != ref->end();) {
        SubroutineDecl *decl = *I;

        // First process the positional parameters.  Move on to the next
        // declaration if is cannot accept the given arguments.
        if (!routineAcceptsArgs(decl, positionalArgs)) {
            I = ref->erase(I);
            continue;
        }

        // Check the keyed arguments for compatability.
        if (!routineAcceptsArgs(decl, keyedArgs)) {
            I = ref->erase(I);
            continue;
        }

        // We have a compatable declaration.
        ++I;
    }

    // If all of the declarations have been filtered out, it is due to ambiguous
    // arguments.
    if (ref->empty()) {
        report(loc, diag::AMBIGUOUS_EXPRESSION);
        return 0;
    }

    // If we have a unique declaration, check the matching call.
    if (ref->isResolved())
        return checkSubroutineCall(ref, positionalArgs, keyedArgs);

    // If we are dealing with procedures the call is ambiguous since we cannot
    // use a return type to resolve any further.
    if (ref->referencesProcedures()) {
        report(loc, diag::AMBIGUOUS_EXPRESSION);
        return 0;
    }

    return new FunctionCallExpr(ref,
                                positionalArgs.data(), numPositional,
                                keyedArgs.data(), numKeys);
}

Ast*
TypeCheck::checkSubroutineCall(SubroutineRef *ref,
                               SVImpl<Expr*>::Type &posArgs,
                               SVImpl<KeywordSelector*>::Type &keyArgs)
{
    assert(ref->isResolved() && "Cannot check call for unresolved reference!");

    Location loc = ref->getLocation();
    SubroutineDecl *decl = ref->getDeclaration();
    unsigned numArgs = posArgs.size() + keyArgs.size();

    if (decl->getArity() != numArgs) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_SUBROUTINE) << decl->getIdInfo();
        return 0;
    }

    if (!checkSubroutineArguments(decl, posArgs, keyArgs))
        return 0;

    if (isa<FunctionDecl>(decl))
        return new FunctionCallExpr(ref,
                                    posArgs.data(), posArgs.size(),
                                    keyArgs.data(), keyArgs.size());
    else
        return new ProcedureCallStmt(ref,
                                     posArgs.data(), posArgs.size(),
                                     keyArgs.data(), keyArgs.size());
}

bool TypeCheck::checkSubroutineArgument(Expr *arg, Type *targetType,
                                        PM::ParameterMode targetMode)
{
    Location argLoc = arg->getLocation();

    if (!checkExprInContext(arg, targetType))
        return false;

    // If the target mode is either "out" or "in out", ensure that the
    // argument provided is compatable.
    if (targetMode == PM::MODE_OUT or targetMode == PM::MODE_IN_OUT) {
        if (DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(arg)) {
            ValueDecl *vdecl = declRef->getDeclaration();
            if (ParamValueDecl *param = dyn_cast<ParamValueDecl>(vdecl)) {
                // If the argument is of mode IN, then so too must be the
                // target mode.
                if (param->getParameterMode() == PM::MODE_IN) {
                    report(argLoc, diag::IN_PARAMETER_NOT_MODE_COMPATABLE)
                        << param->getString() << targetMode;
                    return false;
                }
            }
            else {
                // The only other case (currently) are ObjectDecls, which are
                // always usable.
                assert(isa<ObjectDecl>(vdecl) && "Cannot typecheck decl!");
            }
        }
        else {
            // The argument is not usable in an "out" or "in out" context.
            report(argLoc, diag::EXPRESSION_NOT_MODE_COMPATABLE) << targetMode;
            return false;
        }
    }
    return true;
}

bool
TypeCheck::checkSubroutineArguments(SubroutineDecl *decl,
                                    SVImpl<Expr*>::Type &posArgs,
                                    SVImpl<KeywordSelector*>::Type &keyArgs)
{
    // Check each positional argument.
    typedef SVImpl<Expr*>::Type::iterator pos_iterator;
    pos_iterator PI = posArgs.begin();
    for (unsigned i = 0; PI != posArgs.end(); ++PI, ++i) {
        Expr *arg = *PI;
        Type *targetType = decl->getParamType(i);
        PM::ParameterMode targetMode = decl->getParamMode(i);

        if (!checkSubroutineArgument(arg, targetType, targetMode))
            return false;
    }

    // Check each keyed argument.
    typedef SVImpl<KeywordSelector*>::Type::iterator key_iterator;
    key_iterator KI = keyArgs.begin();
    for ( ; KI != keyArgs.end(); ++KI) {
        KeywordSelector *selector = *KI;
        IdentifierInfo *key = selector->getKeyword();
        Location keyLoc = selector->getLocation();
        Expr *arg = selector->getExpression();
        int keyIndex = decl->getKeywordIndex(key);

        // Ensure the given keyword exists.
        if (keyIndex < 0) {
            report(keyLoc, diag::SUBROUTINE_HAS_NO_SUCH_KEYWORD)
                << key << decl->getIdInfo();
            return false;
        }
        unsigned argIndex = unsigned(keyIndex);

        // The corresponding index of the keyword must be greater than the
        // number of supplied positional parameters (otherwise it would
        // `overlap' a positional parameter).
        if (argIndex < posArgs.size()) {
                report(keyLoc, diag::PARAM_PROVIDED_POSITIONALLY) << key;
                return false;
        }

        // Ensure that this keyword is not a duplicate of any preceding
        // keyword.
        for (key_iterator I = keyArgs.begin(); I != KI; ++I) {
            KeywordSelector *prevSelector = *I;
            if (prevSelector->getKeyword() == key) {
                report(keyLoc, diag::DUPLICATE_KEYWORD) << key;
                return false;
            }
        }

        // Ensure the type of the selected expression is compatible.
        Type *targetType = decl->getParamType(argIndex);
        PM::ParameterMode targetMode = decl->getParamMode(argIndex);
        if (!checkSubroutineArgument(arg, targetType, targetMode))
            return false;
    }
    return true;
}

IndexedArrayExpr *TypeCheck::acceptIndexedArray(DeclRefExpr *ref,
                                                SVImpl<Expr*>::Type &indices)
{
    Location loc = ref->getLocation();
    ValueDecl *vdecl = ref->getDeclaration();
    Type *type = vdecl->getType()->getBaseType();

    if (!isa<ArrayType>(type)) {
        report(loc, diag::EXPECTED_ARRAY_FOR_INDEX);
        return 0;
    }
    ArrayType *arrTy = cast<ArrayType>(type);

    // Check that the number of indices matches the rank of the array type.
    unsigned numIndices = indices.size();
    if (numIndices != arrTy->getRank()) {
        report(loc, diag::WRONG_NUM_SUBSCRIPTS_FOR_ARRAY);
        return 0;
    }

    // Ensure each index is compatible with the arrays type.  If an index does
    // not check, continue checking each remaining index.
    for (unsigned i = 0; i < numIndices; ++i) {
        Expr *index = indices[i];
        if (checkExprInContext(index, arrTy->getIndexType(i)))
            indices.push_back(index);
    }

    // If the number of checked indices does not match the number of nodes
    // given, one or more of the indices did not check.
    if (indices.size() != numIndices)
        return 0;

    // Create a DeclRefExpr to represent the array value and return a fresh
    // expression.
    DeclRefExpr *arrExpr = new DeclRefExpr(vdecl, loc);
    return new IndexedArrayExpr(arrExpr, &indices[0], numIndices);
}

// Typechecks the given expression in the given type context.  This method can
// update the expression (by resolving overloaded function calls, or assigning a
// type to an integer literal, for example).  Returns true if the expression was
// successfully checked.  Otherwise, false is returned and appropriate
// diagnostics are emitted.
bool TypeCheck::checkExprInContext(Expr *expr, Type *context)
{
    // Currently, only two types of expressions are "sensitive" to context,
    // meaning that we might need to patch up the AST so that it conforms to the
    // context -- IntegerLiterals and FunctionCallExpr's.
    if (IntegerLiteral *intLit = dyn_cast<IntegerLiteral>(expr))
        return resolveIntegerLiteral(intLit, context);
    if (FunctionCallExpr *fcall = dyn_cast<FunctionCallExpr>(expr))
        return resolveFunctionCall(fcall, context);

    Type *exprTy = expr->getType();
    assert(exprTy && "Expression does not have a resolved type!");

    // FIXME:  This is a hack.  The basic equality predicate should be able to
    // sort thru this.
    if (CarrierType *carrierTy = dyn_cast<CarrierType>(context)) {
        if (exprTy->equals(carrierTy->getRepresentationType()))
            return true;
    }

    // Otherwise, simply ensure that the given expression is compatable with the
    // context.
    if (exprTy->equals(context))
        return true;
    else {
        // FIXME: Need a better diagnostic here.
        report(expr->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }
}

// Resolves the type of the given integer literal, and ensures that the given
// type context is itself compatible with the literal provided.  Returns true if
// the literal was successfully checked.  Otherwise, false is returned and
// appropriate diagnostics are posted.
bool TypeCheck::resolveIntegerLiteral(IntegerLiteral *intLit, Type *context)
{
    if (intLit->hasType()) {
        assert(intLit->getType()->equals(context) &&
               "Cannot resolve literal to different type!");
        return true;
    }

    if (!context->isIntegerType()) {
        // FIXME: Need a better diagnostic here.
        report(intLit->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }

    // FIXME: Ensure that the literal meets any range constraints implied by the
    // context.
    intLit->setType(context);
    return true;
}

// Resolves the given call expression (which must be nullary function call,
// i.e. one without arguments) to one which satisfies the given target type and
// returns true.  Otherwise, false is returned and the appropriate diagnostics
// are emitted.
bool TypeCheck::resolveNullaryFunctionCall(FunctionCallExpr *call,
                                           Type *targetType)
{
    assert(call->getNumArgs() == 0 &&
           "Call expression has too many arguments!");

    typedef FunctionCallExpr::connective_iterator connective_iter;
    connective_iter iter = call->begin_connectives();
    connective_iter endIter = call->end_connectives();
    FunctionDecl *connective = 0;

    for ( ; iter != endIter; ++iter) {
        FunctionDecl *candidate = *iter;
        Type *returnType = candidate->getReturnType();

        if (targetType->equals(returnType)) {
            if (connective) {
                report(call->getLocation(), diag::AMBIGUOUS_EXPRESSION);
                return false;
            }
            else
                connective = candidate;
        }
    }
    call->resolveConnective(connective);
    return true;
}

// Resolves the given call expression to one which satisfies the given target
// type and returns true.  Otherwise, false is returned and the appropriate
// diagnostics are emitted.
bool TypeCheck::resolveFunctionCall(FunctionCallExpr *call, Type *targetType)
{
    if (!call->isAmbiguous()) {
        // The function call is not ambiguous.  Ensure that the return type of
        // the call and the target type match.
        Type *callTy = call->getType();

        // FIXME: This is a hack.  The type equality predicates should perform
        // these reductions.
        if (CarrierType *carrierTy = dyn_cast<CarrierType>(callTy))
            callTy = carrierTy->getRepresentationType();
        if (CarrierType *carrierTy = dyn_cast<CarrierType>(targetType))
            targetType = carrierTy->getRepresentationType();

        // FIXME: Need a better diagnostic here.
        if (!callTy->equals(targetType)) {
            report(call->getLocation(), diag::INCOMPATIBLE_TYPES);
            return false;
        }
        return true;
    }

    if (call->getNumArgs() == 0)
        return resolveNullaryFunctionCall(call, targetType);

    typedef FunctionCallExpr::connective_iterator connective_iter;
    connective_iter iter = call->begin_connectives();
    connective_iter endIter = call->end_connectives();
    FunctionDecl *fdecl = 0;

    for ( ; iter != endIter; ++iter) {
        FunctionDecl *candidate  = *iter;
        Type *returnType = candidate->getReturnType();
        if (targetType->equals(returnType)) {
            if (fdecl) {
                report(call->getLocation(), diag::AMBIGUOUS_EXPRESSION);
                return false;
            }
            else
                fdecl = candidate;
        }
    }

    // FIXME:  Diagnose that there are no functions of the given name satisfying
    // the target type.
    if (!fdecl) {
        report(call->getLocation(), diag::AMBIGUOUS_EXPRESSION);
        return false;
    }
    else {
        // The declaration has been resolved.  Set it.
        call->resolveConnective(fdecl);
    }

    // Traverse the argument expressions and check each against the types
    // required by the resolved declaration.
    typedef FunctionCallExpr::arg_iterator iterator;
    iterator Iter = call->begin_arguments();
    iterator E = call->end_arguments();
    bool status = true;
    for (unsigned i = 0; Iter != E; ++Iter)
        status = status && checkExprInContext(*Iter, fdecl->getParamType(i));
    return status;
}

Node TypeCheck::acceptInj(Location loc, Node exprNode)
{
    Domoid *domoid = getCurrentDomoid();

    if (!domoid) {
        report(loc, diag::INVALID_INJ_CONTEXT);
        return getInvalidNode();
    }

    // Check that the given expression is of the current domain type.
    DomainType *domTy = domoid->getPercentType();
    Expr *expr = cast_node<Expr>(exprNode);
    if (!checkExprInContext(expr, domTy))
        return getInvalidNode();

    // Check that the carrier type has been defined.
    CarrierDecl *carrier = domoid->getImplementation()->getCarrier();
    if (!carrier) {
        report(loc, diag::CARRIER_TYPE_UNDEFINED);
        return getInvalidNode();
    }

    exprNode.release();
    return getNode(new InjExpr(expr, carrier->getType(), loc));
}

Node TypeCheck::acceptPrj(Location loc, Node exprNode)
{
    Domoid *domoid = getCurrentDomoid();

    if (!domoid) {
        report(loc, diag::INVALID_PRJ_CONTEXT);
        return getInvalidNode();
    }

    // Check that the carrier type has been defined.
    CarrierDecl *carrier = domoid->getImplementation()->getCarrier();
    if (!carrier) {
        report(loc, diag::CARRIER_TYPE_UNDEFINED);
        return getInvalidNode();
    }

    Type *carrierTy = carrier->getType();
    Expr *expr = cast_node<Expr>(exprNode);
    if (!checkExprInContext(expr, carrierTy))
        return getInvalidNode();

    exprNode.release();
    return getNode(new PrjExpr(expr, domoid->getPercentType(), loc));
}

Node TypeCheck::acceptIntegerLiteral(llvm::APInt &value, Location loc)
{
    // The current convention is to represent the values of integer literals as
    // signed APInts such that the bit width of the value can accomidate a two's
    // complement representation.  Since literals are always positive at this
    // stage (we have yet to apply a negation operator, say), zero extend the
    // value by one bit -- this assumes that the parser produces APInt's which
    // are not wider than necessary to represent the unsigned value of the
    // literal, hense the assert.
    assert((value == 0 || value.countLeadingZeros() == 0) &&
           "Unexpected literal representation!");

    if (value != 0)
        value.zext(value.getBitWidth() + 1);

    return getNode(new IntegerLiteral(value, loc));
}
