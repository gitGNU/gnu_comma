//===-- ast/SubroutineCall.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AttribDecl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/SubroutineCall.h"
#include "comma/basic/PrimitiveOps.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

SubroutineCall::SubroutineCall(SubroutineRef *connective,
                               Expr **posArgs, unsigned numPos,
                               KeywordSelector **keyArgs, unsigned numKeys)
    : connective(connective),
      numPositional(numPos),
      numKeys(numKeys)
{
    initializeArguments(posArgs, numPos, keyArgs, numKeys);
}

// FIXME: It would be nice to have a representation where a resolved call
// disposes of the reference node and replaces it directly with the declaration.
SubroutineCall::SubroutineCall(SubroutineDecl *connective,
                               Expr **posArgs, unsigned numPos,
                               KeywordSelector **keyArgs, unsigned numKeys)
    : connective(new SubroutineRef(Location(), connective)),
      numPositional(numPos),
      numKeys(numKeys)
{
    initializeArguments(posArgs, numPos, keyArgs, numKeys);
}

void
SubroutineCall::initializeArguments(Expr **posArgs, unsigned numPos,
                                    KeywordSelector **keyArgs, unsigned numKeys)
{
    unsigned numArgs = numPositional + numKeys;

    if (numArgs) {
        arguments = new Expr*[numArgs];
        std::copy(posArgs, posArgs + numPos, arguments);
        std::fill(arguments + numPos, arguments + numArgs, (Expr*)0);
    }
    else
        arguments = 0;

    if (numKeys) {
        keyedArgs = new KeywordSelector*[numKeys];
        std::copy(keyArgs, keyArgs + numKeys, keyedArgs);
    }
    else
        keyedArgs = 0;

    if (isUnambiguous())
        propagateKeyedArguments();
}

void SubroutineCall::propagateKeyedArguments()
{
    // Fill in the argument vector with any keyed expressions, sorted so that
    // they match what the connective requires.
    for (unsigned i = 0; i < numKeys; ++i) {
        KeywordSelector *selector = keyedArgs[i];
        IdentifierInfo *key = selector->getKeyword();
        Expr *expr = selector->getExpression();
        int indexResult = getConnective()->getKeywordIndex(key);

        assert(indexResult >= 0 && "Could not resolve keyword index!");

        unsigned argIndex = unsigned(indexResult);
        assert(argIndex >= numPositional &&
               "Keyword resolved to a positional index!");
        assert(argIndex < getNumArgs() && "Keyword index too large!");
        assert(arguments[argIndex] == 0 && "Duplicate keywords!");

        arguments[argIndex] = expr;
    }
}

SubroutineCall::~SubroutineCall()
{
    // FIXME: Traverse the argument list and free each expression.
    delete connective;
    delete[] arguments;
    delete[] keyedArgs;
}

bool SubroutineCall::isaFunctionCall() const
{
    return isa<FunctionCallExpr>(this);
}

bool SubroutineCall::isaProcedureCall() const
{
    return isa<ProcedureCallStmt>(this);
}

bool SubroutineCall::denotesOperator() const
{
    if (isaFunctionCall() && isUnambiguous()) {
        IdentifierInfo *idInfo = getConnective()->getIdInfo();
        return PO::denotesOperator(idInfo);
    }
    return false;
}

FunctionCallExpr *SubroutineCall::asFunctionCall()
{
    return dyn_cast<FunctionCallExpr>(this);
}

const FunctionCallExpr *SubroutineCall::asFunctionCall() const
{
    return dyn_cast<FunctionCallExpr>(this);
}

ProcedureCallStmt *SubroutineCall::asProcedureCall()
{
    return dyn_cast<ProcedureCallStmt>(this);
}

const ProcedureCallStmt *SubroutineCall::asProcedureCall() const
{
    return dyn_cast<ProcedureCallStmt>(this);
}

Ast *SubroutineCall::asAst()
{
    if (connective->referencesFunctions())
        return static_cast<FunctionCallExpr*>(this);
    else if (connective->referencesProcedures())
        return static_cast<ProcedureCallStmt*>(this);
    else {
        assert(false && "Cannot infer AST type!");
        return 0;
    }
}

const Ast *SubroutineCall::asAst() const
{
    return const_cast<SubroutineCall*>(this)->asAst();
}

bool SubroutineCall::isCompatible(SubroutineDecl *decl) const
{
    if (isa<FunctionDecl>(decl))
        return isaFunctionCall();
    else
        return isaProcedureCall();
}

void SubroutineCall::resolveConnective(SubroutineDecl *decl)
{
    assert(isCompatible(decl) &&
           "Subroutine not compatible with this kind of call!");
    assert(decl->getArity() == getNumArgs() && "Arity mismatch!");

    connective->resolve(decl);
    propagateKeyedArguments();
}

int SubroutineCall::argExprIndex(Expr *expr) const
{
    unsigned numArgs = getNumArgs();
    for (unsigned i = 0; i < numArgs; ++i)
        if (arguments[i] == expr)
            return i;
    return -1;
}

int SubroutineCall::keyExprIndex(Expr *expr) const
{
    for (unsigned i = 0; i < numKeys; ++i)
        if (keyedArgs[i]->getExpression() == expr)
            return i;
    return -1;
}

void SubroutineCall::setArgument(arg_iterator I, Expr *expr)
{
    int index = argExprIndex(*I);
    assert(index >= 0 && "Iterator does not point to an argument!");

    arguments[index] = expr;

    if ((index = keyExprIndex(*I)) >= 0)
        keyedArgs[index]->setRHS(expr);
}

void SubroutineCall::setArgument(key_iterator I, Expr *expr)
{
    Expr *target = (*I)->getExpression();
    int index = keyExprIndex(target);
    assert(index >= 0 && "Iterator does not point to an argument!");

    keyedArgs[index]->setRHS(expr);

    if ((index = argExprIndex(target)) >= 0)
        arguments[index] = expr;
}

bool SubroutineCall::isAttributeCall() const
{
    return isUnambiguous() && isa<FunctionAttribDecl>(getConnective());
}

bool SubroutineCall::isDirectCall() const
{
    if (isAmbiguous())
        return false;

    const SubroutineDecl *decl = getConnective();
    const DeclRegion *region = decl->getDeclRegion();
    return isa<PkgInstanceDecl>(region);
}

bool SubroutineCall::isLocalCall() const
{
    if (isAmbiguous())
        return false;

    // If the declarative region maps to an add or package decl then this is a
    // local call.
    const SubroutineDecl *decl = getConnective();
    const DeclRegion *region = decl->getDeclRegion();
    return (isa<BodyDecl>(region) || isa<PackageDecl>(region));
}

bool SubroutineCall::isForeignCall() const
{
    if (isAmbiguous())
        return false;

    const SubroutineDecl *srDecl = getConnective();
    return srDecl->hasPragma(pragma::Import);
}
