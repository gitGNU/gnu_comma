//===-- typecheck/CheckExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/typecheck/TypeCheck.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/Casting.h"

#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


Node TypeCheck::acceptQualifier(Node typeNode, Location loc)
{
    Type *domain = ensureDomainType(typeNode, loc);

    if (domain) {
        typeNode.release();
        return getNode(new Qualifier(domain, loc));
    }
    else
        return getInvalidNode();
}

// FIXME: Implement.
Node TypeCheck::acceptNestedQualifier(Node     qualifierNode,
                                      Node     typeNode,
                                      Location loc)
{
    assert(false && "Nested qualifiers not yet implemented!");
    return getInvalidNode();
}

// This function is a helper to acceptDirectName.  It checks that an arbitrary
// decl denotes a direct name (a value decl or nullary function).  Returns an
// expression node corresponding to the given candidate when accepted, otherwise
// 0 is returned.
Expr *TypeCheck::resolveDirectDecl(Decl           *candidate,
                                   IdentifierInfo *name,
                                   Location        loc)
{
    if (isa<ValueDecl>(candidate)) {
        ValueDecl  *decl = cast<ValueDecl>(candidate);
        DeclRefExpr *ref = new DeclRefExpr(decl, loc);
        return ref;
    }

    if (isa<FunctionDecl>(candidate)) {
        FunctionDecl *decl = cast<FunctionDecl>(candidate);
        if (decl->getArity() == 0) {
            FunctionCallExpr *call = new FunctionCallExpr(decl, 0, 0, loc);
            return call;
        }
    }

    return 0;
}

// FIXME: This function is just an example of where name lookup is going.  The
// logic herein needs to be factored out appropriately.
Node TypeCheck::acceptDirectName(IdentifierInfo *name, Location loc)
{
    Homonym *homonym = name->getMetadata<Homonym>();

    if (!homonym || homonym->empty()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    // Examine the direct declarations for a value of the given name.
    for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
         iter != homonym->endDirectDecls(); ++iter) {
        if (Expr *expr = resolveDirectDecl(*iter, name, loc))
            return getNode(expr);
    }

    // Otherwise, scan the full set of imported declarations, and partition the
    // import decls into two sets:  one containing all value declarations, the
    // other containing all nullary function declarations.
    llvm::SmallVector<FunctionDecl*, 4> functionDecls;
    llvm::SmallVector<ValueDecl*, 4>    valueDecls;

    for (Homonym::ImportIterator iter = homonym->beginImportDecls();
         iter != homonym->endImportDecls(); ++iter) {
        Decl *candidate = *iter;
        if (ValueDecl *decl = dyn_cast<ValueDecl>(candidate))
            valueDecls.push_back(decl);
        else if (FunctionDecl *decl = dyn_cast<FunctionDecl>(candidate))
            if (decl->getArity() == 0) functionDecls.push_back(decl);
    }

    // If both partitions are empty, then no name is visible.
    if (valueDecls.empty() && functionDecls.empty()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    if (valueDecls.empty()) {
        // Possibly several nullary functions are visible.  We cannot resolve
        // further so build a function call with an overloaded set of
        // connectives.
        //
        // FIXME: We could check that the set collected admits at least two
        // distinct return types.
        FunctionCallExpr *call =
            new FunctionCallExpr(functionDecls[0], 0, 0, loc);
        for (unsigned i = 1; i < functionDecls.size(); ++i)
            call->addConnective(functionDecls[i]);
        return getNode(call);
    }
    else if (functionDecls.empty()) {
        // If a there is more than one value decl in effect, then we have an
        // ambiguity.  Value decls are not overloadable.
        if (valueDecls.size() > 1) {
            report(loc, diag::AMBIGUOUS_EXPRESSION);
            return getInvalidNode();
        }
        // Otherwise, we can fully resolve the value.
        DeclRefExpr *ref = new DeclRefExpr(valueDecls[0], loc);
        return getNode(ref);
    }
    else {
        // There are both values and nullary function declarations in scope.
        // Since values are not overloadable entities we have a conflict and
        // this expression requires qualification.
        report(loc, diag::MULTIPLE_IMPORT_AMBIGUITY);
        return getInvalidNode();
    }
}

Node TypeCheck::acceptQualifiedName(Node            qualNode,
                                    IdentifierInfo *name,
                                    Location        loc)
{
    Qualifier         *qualifier = cast_node<Qualifier>(qualNode);
    DeclarativeRegion *region    = qualifier->resolve();

    // Lookup the name in the resolved declarative region.
    typedef DeclarativeRegion::PredRange PredRange;
    typedef DeclarativeRegion::PredIter  PredIter;

    PredRange range = region->findDecls(name);

    if (range.first == range.second) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    // Currently, the only component of a domain which we support are function
    // decls.  Collect the nullary functions.
    llvm::SmallVector<FunctionDecl*, 4> functionDecls;

    for (PredIter iter = range.first; iter != range.second; ++iter) {
        if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(*iter)) {
            if (fdecl->getArity() == 0)
                functionDecls.push_back(fdecl);
        }
    }

    // FIXME: Report that there are no nullary functions declared in this
    // domain.
    if (functionDecls.empty()) {
        report(loc, diag::NAME_NOT_VISIBLE);
        return getInvalidNode();
    }

    // Form the function call node.
    FunctionCallExpr *call = new FunctionCallExpr(functionDecls[0], 0, 0, loc);
    for (unsigned i = 1; i < functionDecls.size(); ++i)
        call->addConnective(functionDecls[i]);
    call->setQualifier(qualifier);
    qualNode.release();
    return getNode(call);
}

Node TypeCheck::acceptFunctionCall(IdentifierInfo *name,
                                   Location        loc,
                                   NodeVector     &argNodes)
{
    return acceptSubroutineCall(name, loc, argNodes, true);
}

// Note that this function will evolve to take a Node in place of an identifier,
// and will be renamed to acceptFunctionCall.
Node TypeCheck::acceptSubroutineCall(IdentifierInfo *name,
                                     Location        loc,
                                     NodeVector     &argNodes,
                                     bool            checkFunction)
{
    llvm::SmallVector<Expr*, 8> args;
    unsigned numArgs = argNodes.size();

    // Convert the argument nodes to Expr's and release the Node's.
    for (unsigned i = 0; i < numArgs; ++i) {
        args.push_back(cast_node<Expr>(argNodes[i]));
    }

    llvm::SmallVector<SubroutineDecl*, 8> routineDecls;
    Homonym *homonym = name->getMetadata<Homonym>();

    if (!homonym) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    lookupSubroutineDecls(homonym, numArgs, routineDecls, checkFunction);

    if (routineDecls.empty()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    if (routineDecls.size() == 1)
        return checkSubroutineCall(routineDecls[0], loc, &args[0], numArgs);

    // We use the following bit vector to indicate which elements of the
    // routineDecl vector are applicable as we resolve the subroutine call with
    // respect to the given arguments.
    llvm::BitVector declFilter(routineDecls.size(), true);

    // First, reduce the set of declarations to include only those which can
    // accept any keyword selectors provided.
    for (unsigned i = 0; i < routineDecls.size(); ++i) {
        SubroutineDecl *decl = routineDecls[i];
        for (unsigned j = 0; j < numArgs; ++j) {
            Expr *arg = args[j];
            if (KeywordSelector *selector = dyn_cast<KeywordSelector>(arg)) {
                IdentifierInfo *key      = selector->getKeyword();
                Location        keyLoc   = selector->getLocation();
                int             keyIndex = decl->getKeywordIndex(key);
                if (keyIndex < 0) {
                    declFilter[i] = false;
                    if (declFilter.none()) {
                        report(keyLoc, diag::SUBROUTINE_HAS_NO_SUCH_KEYWORD)
                            << key << name;
                        return getInvalidNode();
                    }
                    break;
                }
            }
        }
    }

    // Reduce the set of declarations with respect to the types of its
    // arguments.
    for (unsigned i = 0; i < routineDecls.size(); ++i) {
        SubroutineDecl *decl = routineDecls[i];
        for (unsigned j = 0; j < numArgs && declFilter[i]; ++j) {
            Expr     *arg         = args[j];
            unsigned  targetIndex = j;

            if (KeywordSelector *selector = dyn_cast<KeywordSelector>(args[j])) {
                arg         = selector->getExpression();
                targetIndex = decl->getKeywordIndex(selector->getKeyword());
            }

            Type *targetType = decl->getArgType(targetIndex);

            // If the argument as a fully resolved type, all we currently do is
            // test for type equality.
            if (arg->hasType()) {
                if (!targetType->equals(arg->getType())) {
                    declFilter[i] = false;
                    // If the set of applicable declarations has been reduced to
                    // zero, report this argument as ambiguous.
                    if (declFilter.none())
                        report(arg->getLocation(), diag::AMBIGUOUS_EXPRESSION);
                }
                continue;
            }

            // Otherwise, we have an unresolved argument expression (which must
            // be a function call expression).
            typedef FunctionCallExpr::ConnectiveIterator ConnectiveIter;
            FunctionCallExpr *argCall = cast<FunctionCallExpr>(arg);
            bool   applicableArgument = false;

            // Check if at least one interpretation of the argument satisfies
            // the current target type.
            for (ConnectiveIter iter = argCall->beginConnectives();
                 iter != argCall->endConnectives(); ++iter) {
                FunctionDecl *connective = *iter;
                Type         *returnType = connective->getReturnType();
                if (targetType->equals(returnType)) {
                    applicableArgument = true;
                    break;
                }
            }

            // If this argument is not applicable (meaning, there is no
            // interpretation of the argument for this particular decl), filter
            // out the decl.
            if (!applicableArgument) {
                declFilter[i] = false;
                if (declFilter.none())
                    report(arg->getLocation(), diag::AMBIGUOUS_EXPRESSION);
            }
        }
    }

    // If all of the declarations have been filtered out, it is due to ambiguous
    // arguments.  Simply return.
    if (declFilter.none())
        return getInvalidNode();

    // If we have a unique declaration, check the matching call.
    if (declFilter.count() == 1) {
        argNodes.release();
        SubroutineDecl *decl = routineDecls[declFilter.find_first()];
        Node result = checkSubroutineCall(decl, loc, &args[0], numArgs);
        if (result.isValid())
            argNodes.release();
        return result;
    }

    // If we are dealing with functions, the resolution of the call will depend
    // on the resolution of the return type.  If we are dealing with procedures,
    // then the call is ambiguous.
    if (checkFunction) {
        llvm::SmallVector<FunctionDecl*, 4> connectives;
        for (unsigned i = 0; i < routineDecls.size(); ++i)
            if (declFilter[i]) {
                FunctionDecl *fdecl = cast<FunctionDecl>(routineDecls[i]);
                connectives.push_back(fdecl);
            }
        FunctionCallExpr *call =
            new FunctionCallExpr(connectives[0], &args[0], numArgs, loc);
        for (unsigned i = 1; i < connectives.size(); ++i)
            call->addConnective(connectives[i]);
        argNodes.release();
        return getNode(call);
    }
    else {
        report(loc, diag::AMBIGUOUS_EXPRESSION);
        return getInvalidNode();
    }
}

// This function looks up the set of visible subroutines of a certain arity in
// the given homonym and populates the vector routineDecls with the results.  If
// lookupFunctions is true, this method scans for functions, otherwise for
// procedures.
void TypeCheck::lookupSubroutineDecls(
    Homonym *homonym,
    unsigned arity,
    llvm::SmallVector<SubroutineDecl*, 8> &routineDecls,
    bool lookupFunctions)
{
    SubroutineDecl *decl;       // Declarations provided by the homonym.
    SubroutineType *type;       // Type of `decl'.
    SubroutineType *shadowType; // Type of previous direct lookups.

    if (homonym->empty())
        return;

    // Accumulate any direct declarations.
    for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
         iter != homonym->endDirectDecls(); ++iter) {
        if (lookupFunctions)
            decl = dyn_cast<FunctionDecl>(*iter);
        else
            decl = dyn_cast<ProcedureDecl>(*iter);
        if (decl && decl->getArity() == arity) {
            type = decl->getType();
            for (unsigned i = 0; i < routineDecls.size(); ++i) {
                shadowType = routineDecls[i]->getType();
                if (shadowType->equals(type)) {
                    type = 0;
                    break;
                }
            }
            if (type)
                routineDecls.push_back(decl);
        }
    }

    // Accumulate import declarations, ensuring that any directly visible
    // declarations shadow those imports with matching types.  Imported
    // declarations do not shadow each other.
    unsigned numDirectDecls = routineDecls.size();
    for (Homonym::ImportIterator iter = homonym->beginImportDecls();
         iter != homonym->endImportDecls(); ++iter) {
        if (lookupFunctions)
            decl = dyn_cast<FunctionDecl>(*iter);
        else
            decl = dyn_cast<ProcedureDecl>(*iter);
        if (decl && decl->getArity() == arity) {
            type = decl->getType();
            for (unsigned i = 0; i < numDirectDecls; ++i) {
                shadowType = routineDecls[i]->getType();
                if (shadowType->equals(type)) {
                    type = 0;
                    break;
                }
            }
            if (type)
                routineDecls.push_back(decl);
        }
    }
}

Node TypeCheck::checkSubroutineCall(SubroutineDecl  *decl,
                                    Location         loc,
                                    Expr           **args,
                                    unsigned         numArgs)

{
    if (decl->getArity() != numArgs) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_SUBROUTINE) << decl->getIdInfo();
        return getInvalidNode();
    }

    llvm::SmallVector<Expr*, 4> sortedArgs(numArgs);
    unsigned numPositional = 0;

    // Sort the arguments wrt the functions keyword profile.
    for (unsigned i = 0; i < numArgs; ++i) {
        Expr *arg = args[i];

        if (KeywordSelector *selector = dyn_cast<KeywordSelector>(arg)) {
            IdentifierInfo  *key      = selector->getKeyword();
            Location         keyLoc   = selector->getLocation();
            int              keyIdx   = decl->getKeywordIndex(key);

            // Ensure the given keyword exists.
            if (keyIdx < 0) {
                report(keyLoc, diag::SUBROUTINE_HAS_NO_SUCH_KEYWORD)
                    << key << decl->getIdInfo();
                return getInvalidNode();
            }

            // The corresponding index of the keyword must be greater than the
            // number of supplied positional parameters (otherwise it would
            // `overlap' a positional parameter).
            if ((unsigned)keyIdx < numPositional) {
                report(keyLoc, diag::PARAM_PROVIDED_POSITIONALLY) << key;
                return getInvalidNode();
            }

            // Ensure that this keyword is not a duplicate of any preceding
            // keyword.
            for (unsigned j = numPositional; j < i; ++j) {
                KeywordSelector *prevSelector;
                prevSelector = cast<KeywordSelector>(args[j]);

                if (prevSelector->getKeyword() == key) {
                    report(keyLoc, diag::DUPLICATE_KEYWORD) << key;
                    return getInvalidNode();
                }
            }

            // Add the argument in its proper position.
            sortedArgs[keyIdx] = arg;
        }
        else {
            numPositional++;
            sortedArgs[i] = arg;
        }
    }

    // Check each argument types wrt this decl.
    for (unsigned i = 0; i < numArgs; ++i) {
        Type *targetType = decl->getArgType(i);
        Expr *arg        = sortedArgs[i];

        if (KeywordSelector *selector = dyn_cast<KeywordSelector>(arg))
            arg = selector->getExpression();

        if (!ensureExprType(arg, targetType))
            return getInvalidNode();
    }

    if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(decl)) {
        FunctionCallExpr *call =
            new FunctionCallExpr(fdecl, &sortedArgs[0], numArgs, loc);
        return getNode(call);
    }
    else {
        ProcedureDecl *pdecl = cast<ProcedureDecl>(decl);
        ProcedureCallStmt *call =
            new ProcedureCallStmt(pdecl, &sortedArgs[0], numArgs, loc);
        return getNode(call);
    }
}

// Resolves the given call expression (which should have multiple candidate
// connectives) to one which satisfies the given target type and returns true.
// Otherwise, false is returned and the appropriated diagnostics are emitted.
bool TypeCheck::resolveFunctionCall(FunctionCallExpr *call, Type *targetType)
{
    typedef FunctionCallExpr::ConnectiveIterator ConnectiveIter;
    ConnectiveIter iter    = call->beginConnectives();
    ConnectiveIter endIter = call->endConnectives();
    FunctionDecl  *fdecl   = 0;

    for ( ; iter != endIter; ++iter) {
        FunctionDecl *candidate  = *iter;
        Type         *returnType = candidate->getReturnType();
        if (targetType->equals(returnType)) {
            if (fdecl) {
                report(call->getLocation(), diag::AMBIGUOUS_EXPRESSION);
                return false;
            }
            else
                fdecl = candidate;
        }
    }

    // Traverse the argument set, patching up any unresolved argument
    // expressions.  We also need to sort the arguments according to the keyword
    // selections, since the connectives do not necessarily respect a uniform
    // ordering.
    bool     status  = true;
    unsigned numArgs = call->getNumArgs();
    llvm::SmallVector<Expr*, 8> sortedArgs(numArgs);

    for (unsigned i = 0; i < call->getNumArgs(); ++i) {
        FunctionCallExpr *argCall;
        unsigned          argIndex = i;
        Expr             *arg      = call->getArg(i);

        // If we have a keyword selection, locate the corresponding index, and
        // resolve the selection to its corresponding expression.
        if (KeywordSelector *select = dyn_cast<KeywordSelector>(arg)) {
            arg      = select->getExpression();
            argIndex = fdecl->getKeywordIndex(select->getKeyword());
            sortedArgs[argIndex] = select;
        }
        else
            sortedArgs[argIndex] = arg;

        argCall = dyn_cast<FunctionCallExpr>(arg);
        if (argCall && argCall->isAmbiguous())
            status = status &&
                resolveFunctionCall(argCall, fdecl->getArgType(argIndex));
    }
    call->setConnective(fdecl);
    return status;
}

bool TypeCheck::ensureExprType(Expr *expr, Type *targetType)
{
    if (expr->hasType()) {
        if (targetType->equals(expr->getType()))
            return true;
        report(expr->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }
    else {
        // Otherwise, the expression must be a function call overloaded on the
        // return type.
        FunctionCallExpr *fcall = cast<FunctionCallExpr>(expr);
        return resolveFunctionCall(fcall, targetType);
    }
}

Node TypeCheck::acceptInj(Location loc, Node exprNode)
{
    Expr   *expr   = cast_node<Expr>(exprNode);
    Domoid *domoid = getCurrentDomoid();

    if (!domoid) {
        report(loc, diag::INVALID_INJ_CONTEXT);
        return getInvalidNode();
    }

    // Check that the given expression is of the current domain type.
    DomainType *domTy = domoid->getPercent();
    Type      *exprTy = expr->getType();
    if (!domTy->equals(exprTy)) {
        report(loc, diag::INCOMPATIBLE_TYPES);
        return getInvalidNode();
    }

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
    Expr   *expr   = cast_node<Expr>(exprNode);
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

    // Check that the given expression is of the carrier type.
    Type *carrierTy = carrier->getType();
    Type *exprTy    = expr->getType();
    if (!carrierTy->equals(exprTy)) {
        report(loc, diag::INCOMPATIBLE_TYPES);
        return getInvalidNode();
    }

    exprNode.release();
    return getNode(new PrjExpr(expr, domoid->getPercent(), loc));
}
