//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"
#include "comma/typecheck/TypeCheck.h"
#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

Node TypeCheck::acceptProcedureCall(IdentifierInfo  *name,
                                    Location         loc,
                                    Node            *args,
                                    unsigned         numArgs)
{
    return acceptSubroutineCall(name, loc, args, numArgs, false);
}

Node TypeCheck::acceptReturnStmt(Location loc, Node retNode)
{
    assert((checkingProcedure() || checkingFunction()) &&
           "Return statement outside subroutine context!");

    if (retNode.isNull()) {
        if (checkingProcedure())
            return Node(new ReturnStmt(loc));

        report(loc, diag::EMPTY_RETURN_IN_FUNCTION);
        return Node::getInvalidNode();
    }
    else {
        if (checkingFunction()) {
            FunctionDecl *fdecl      = getCurrentFunction();
            Expr         *retExpr    = cast_node<Expr>(retNode);
            Type         *targetType = fdecl->getReturnType();

            if (ensureExprType(retExpr, targetType))
                return Node(new ReturnStmt(loc, retExpr));
            return Node::getInvalidNode();
        }

        report(loc, diag::NONEMPTY_RETURN_IN_PROCEDURE);
        return Node::getInvalidNode();
    }
}

Node TypeCheck::acceptAssignmentStmt(Location        loc,
                                     IdentifierInfo *name,
                                     Node            valueNode)
{
    Expr      *value      = cast_node<Expr>(valueNode);
    Homonym   *homonym    = name->getMetadata<Homonym>();
    ValueDecl *targetDecl = 0;

    if (!homonym || homonym->empty()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return Node::getInvalidNode();
    }

    // FIXME: For now, lookup a lexical names only.  Revisit the issue of
    // assignment to an exported name later.
    for (Homonym::DirectIterator iter = homonym->beginDirectDecls();
         iter != homonym->endDirectDecls(); ++iter) {
        if ((targetDecl = dyn_cast<ValueDecl>(*iter)))
            break;
    }

    if (!targetDecl) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return Node::getInvalidNode();
    }

    if (ensureExprType(value, targetDecl->getType())) {
        DeclRefExpr *ref = new DeclRefExpr(targetDecl, loc);
        return Node(new AssignmentStmt(ref, value));
    }

    return Node::getInvalidNode();
}

Node TypeCheck::acceptIfStmt(Location loc,
                             Node     conditionNode,
                             Node    *consequentNodes,
                             unsigned numConsequents)
{
    Expr *condition   = cast_node<Expr>(conditionNode);
    Type *conditionTy = condition->getType();

    if (conditionTy->equals(theBoolDecl->getType())) {
        StmtSequence *consequents = new StmtSequence();
        for (unsigned i = 0; i < numConsequents; ++i)
            consequents->addStmt(cast_node<Stmt>(consequentNodes[i]));

        return Node(new IfStmt(loc, condition, consequents));
    }
    return Node::getInvalidNode();
}

Node TypeCheck::acceptElseStmt(Location loc, Node ifNode,
                               Node *alternateNodes, unsigned numAlternates)
{
    IfStmt       *cond       = cast_node<IfStmt>(ifNode);
    StmtSequence *alternates = new StmtSequence();

    assert(!cond->hasAlternate() && "Multiple else component in IfStmt!");

    for (unsigned i = 0; i < numAlternates; ++i)
        alternates->addStmt(cast_node<Stmt>(alternateNodes[i]));

    cond->setAlternate(loc, alternates);
    return ifNode;
}

Node TypeCheck::acceptElsifStmt(Location loc,
                                Node     ifNode,
                                Node     conditionNode,
                                Node    *consequentNodes,
                                unsigned numConsequents)
{
    IfStmt *cond        = cast_node<IfStmt>(ifNode);
    Expr   *condition   = cast_node<Expr>(conditionNode);
    Type   *conditionTy = condition->getType();

    if (conditionTy->equals(theBoolDecl->getType())) {
        StmtSequence *consequents = new StmtSequence();
        for (unsigned i = 0; i < numConsequents; ++i)
            consequents->addStmt(cast_node<Stmt>(consequentNodes[i]));
        cond->addElsif(loc, condition, consequents);
        return ifNode;
    }
    return Node::getInvalidNode();
}

