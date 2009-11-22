//===-- typecheck/CheckCall.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Routines specific to the checking of subroutine calls.
//===----------------------------------------------------------------------===//

#include "TypeCheck.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Stmt.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Returns true if the given subroutine declaration accepts the given keyword
/// selectors, assuming \p numPositional positional parameters are present.
bool routineAcceptsKeywords(SubroutineDecl *decl,
                            unsigned numPositional,
                            llvm::SmallVectorImpl<KeywordSelector*> &keys)
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

/// Builds either a function call or procedure call node depending on the
/// contents of the given SubroutineRef.
SubroutineCall *
makeSubroutineCall(SubroutineRef *ref,
                   Expr **positionalArgs, unsigned numPositional,
                   KeywordSelector **keyedArgs, unsigned numKeys)
{
    assert(!ref->empty() && "Empty subroutine reference!");

    if (ref->referencesFunctions())
        return new FunctionCallExpr(ref, positionalArgs, numPositional,
                                    keyedArgs, numKeys);
    else
        return new ProcedureCallStmt(ref, positionalArgs, numPositional,
                                     keyedArgs, numKeys);
}

/// Injects implicit ConversionExpr nodes into the positional and keyword
/// arguments to reflect any conversions needed to properly form a call to the
/// given subroutine.
void
convertSubroutineArguments(SubroutineDecl *decl,
                           llvm::SmallVectorImpl<Expr*> &posArgs,
                           llvm::SmallVectorImpl<KeywordSelector*> &keyArgs)
{
    typedef llvm::SmallVectorImpl<Expr*>::iterator pos_iterator;
    pos_iterator PI = posArgs.begin();
    for (unsigned i = 0; PI != posArgs.end(); ++PI, ++i) {
        Expr *arg = *PI;
        Type *targetType = decl->getParamType(i);
        if (TypeCheck::conversionRequired(arg->getType(), targetType))
            *PI = new ConversionExpr(arg, targetType);
    }

    typedef llvm::SmallVectorImpl<KeywordSelector*>::iterator key_iterator;
    key_iterator KI = keyArgs.begin();
    for ( ; KI != keyArgs.end(); ++KI) {
        KeywordSelector *selector = *KI;
        Expr *arg = selector->getExpression();
        unsigned index = unsigned(decl->getKeywordIndex(selector));
        Type *targetType = decl->getParamType(index);
        if (arg->getType() != targetType)
            selector->setRHS(new ConversionExpr(arg, targetType));
    }
}

/// Injects implicit ConversionExpr nodes into the argument set of the given
/// SubroutineCall to reflect any conversions needed to properly form a call to
/// the given subroutine.
void convertSubroutineCallArguments(SubroutineCall *call)
{
    assert(call->isUnambiguous() && "Expected resolved call!");

    typedef SubroutineCall::arg_iterator iterator;
    iterator I = call->begin_arguments();
    iterator E = call->end_arguments();
    SubroutineDecl *decl = call->getConnective();
    for (unsigned i = 0; I != E; ++I, ++i) {
        Expr *arg = *I;
        Type *targetType = decl->getParamType(i);
        if (TypeCheck::conversionRequired(arg->getType(), targetType))
            call->setArgument(I, new ConversionExpr(arg, targetType));
    }
}

} // End anonymous namespace.

bool TypeCheck::checkApplicableArgument(Expr *arg, Type *targetType)
{
    // If the argument as a fully resolved type, all we currently do is test for
    // type equality.
    if (arg->hasType())
        return covers(targetType, arg->getType());

    // We have an unresolved argument expression.  If the expression is an
    // integer literal it is compatable if the target is an integer type.
    if (isa<IntegerLiteral>(arg))
        return targetType->isIntegerType();

    // The expression must be an ambiguous function call.  Check that at least
    // one interpretation of the call satisfies the target type.
    typedef FunctionCallExpr::fun_iterator iterator;
    FunctionCallExpr *call = cast<FunctionCallExpr>(arg);

    iterator I = call->begin_functions();
    iterator E = call->end_functions();
    for ( ; I != E; ++I) {
        FunctionDecl *connective = *I;
        Type *returnType = connective->getReturnType();
        if (covers(targetType, returnType))
            return true;
    }
    return false;
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

Ast* TypeCheck::acceptSubroutineCall(SubroutineRef *ref,
                                     SVImpl<Expr*>::Type &positionalArgs,
                                     SVImpl<KeywordSelector*>::Type &keyedArgs)
{
    Location loc = ref->getLocation();
    unsigned numPositional = positionalArgs.size();
    unsigned numKeys = keyedArgs.size();

    // If there is only one interpretation as a call check it immediately and
    // return a resolved call node if successful.
    if (ref->isResolved()) {
        SubroutineCall *call;
        call = checkSubroutineCall(ref, positionalArgs, keyedArgs);
        return call ? call->asAst() : 0;
    }

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
    if (ref->isResolved()) {
        SubroutineCall *call;
        call = checkSubroutineCall(ref, positionalArgs, keyedArgs);
        return call ? call->asAst() : 0;
    }

    // If we are dealing with procedures the call is ambiguous since we cannot
    // use a return type to resolve any further.
    if (ref->referencesProcedures()) {
        report(loc, diag::AMBIGUOUS_EXPRESSION);
        return 0;
    }

    SubroutineCall *call =
        makeSubroutineCall(ref,
                           positionalArgs.data(), numPositional,
                           keyedArgs.data(), numKeys);
    return call->asAst();
}

FunctionDecl *TypeCheck::resolvePreferredConnective(FunctionCallExpr *call,
                                                    Type *targetType)
{
    typedef FunctionCallExpr::fun_iterator iterator;

    // Build a vector of candidate declarations which are covered by the target
    // type.
    llvm::SmallVector<FunctionDecl *, 8> candidates;
    iterator I = call->begin_functions();
    iterator E = call->end_functions();
    for ( ; I != E; ++I) {
        FunctionDecl *candidate = *I;
        Type *returnType = candidate->getReturnType();
        if (covers(targetType, returnType))
            candidates.push_back(candidate);
    }

    // If there are no candidate declarations we cannot resolve this call.  If
    // there is one candidate, the call is resolved.  If there is more than one
    // candidate, attempt to refine even further by showing preference to the
    // primitive operators.
    FunctionDecl *preference;
    if (candidates.empty())
        preference = 0;
    else if (candidates.size() == 1)
        preference = candidates.front();
    else
        preference = resolvePreferredOperator(candidates);
    return preference;
}

FunctionDecl *
TypeCheck::resolvePreferredOperator(SVImpl<FunctionDecl*>::Type &decls)
{
    // Walk the set of connectives and check that each possible interpretation
    // denotes a primitive operator, and find one of the operators provided by
    // root_integer.
    IntegerDecl *theRootInteger = resource.getTheRootIntegerDecl();
    FunctionDecl *preference = 0;

    typedef SVImpl<FunctionDecl*>::Type::iterator iterator;
    iterator I = decls.begin();
    iterator E = decls.end();
    for ( ; I != E; ++I) {
        FunctionDecl *candidate = *I;
        if (candidate->isPrimitive()) {
            if (candidate->isDeclaredIn(theRootInteger)) {
                // We have a prefered function.  We should never get more than
                // one match.
                assert(preference == 0 && "More than one prefered decl!");
                preference = candidate;
            }
        }
        else {
            // There are non-primitive operations. We cannot prefer a primitive
            // operator in this case.
            return 0;
        }
    }
    return preference;
}

SubroutineCall *
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

    convertSubroutineArguments(decl, posArgs, keyArgs);
    return makeSubroutineCall(ref, posArgs.data(), posArgs.size(),
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
    if (targetMode == PM::MODE_OUT || targetMode == PM::MODE_IN_OUT) {
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

bool TypeCheck::checkSubroutineCallArguments(SubroutineCall *call)
{
    assert(call->isUnambiguous() && "Expected unambiguous call!");

    typedef SubroutineCall::arg_iterator iterator;
    iterator I = call->begin_arguments();
    iterator E = call->end_arguments();
    bool status = true;
    SubroutineDecl *decl = call->getConnective();

    for (unsigned i = 0; I != E; ++I, ++i) {
        PM::ParameterMode targetMode = decl->getParamMode(i);
        Type *targetType = decl->getParamType(i);
        status = status && checkSubroutineArgument(*I, targetType, targetMode);
    }
    return status;
}

bool TypeCheck::resolveFunctionCall(FunctionCallExpr *call, Type *targetType)
{
    if (!call->isAmbiguous()) {
        // The function call is not ambiguous.  Ensure that the return type of
        // the call is covered by the target type.
        if (covers(targetType, call->getType()))
            return true;

        // FIXME: Need a better diagnostic here.
        report(call->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }

    FunctionDecl *preference = resolvePreferredConnective(call, targetType);

    if (!preference)  {
        // FIXME:  Actually print something informative here.
        report(call->getLocation(), diag::AMBIGUOUS_EXPRESSION);
        return false;
    }

    // Resolve the call and check its final interpretation.  Inject any implicit
    // conversions needed by the arguments.
    call->resolveConnective(preference);
    if (!checkSubroutineCallArguments(call))
        return false;
    convertSubroutineCallArguments(call);
    return true;
}

bool TypeCheck::resolveFunctionCall(FunctionCallExpr *call,
                                    Type::Classification ID)
{
    typedef FunctionCallExpr::fun_iterator iterator;

    if (!call->isAmbiguous()) {
        // The function call is not ambiguous.  Ensure that the return type of
        // the call is a member of the target classification.
        if (call->getType()->memberOf(ID))
            return true;

        // FIXME: Need a better diagnostic here.
        report(call->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }

    llvm::SmallVector<FunctionDecl*, 8> candidates;
    iterator I = call->begin_functions();
    iterator E = call->end_functions();
    for ( ; I != E; ++I) {
        FunctionDecl *candidate  = *I;
        Type *returnType = candidate->getReturnType();
        if (returnType->memberOf(ID))
            candidates.push_back(candidate);
    }

    // If there are no candidate declarations we cannot resolve this call.  If
    // there is one candidate the call is resolved.  If there is more than one
    // candidate, attempt to refine even further by showing preference to the
    // primitive operators.
    FunctionDecl *preference = 0;
    if (candidates.empty())
        preference = 0;
    else if (candidates.size() == 1)
        preference = candidates.front();
    else
        preference = resolvePreferredOperator(candidates);

    if (!preference) {
        // FIXME: Actually print an informative diagnostic.
        report(call->getLocation(), diag::AMBIGUOUS_EXPRESSION);
        return false;
    }

    // Resolve the call and check its final interpretation.  Inject any implicit
    // conversions needed by the arguments.
    call->resolveConnective(preference);
    if (!checkSubroutineCallArguments(call))
        return false;
    convertSubroutineCallArguments(call);
    return true;
}
