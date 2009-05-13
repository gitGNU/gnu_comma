//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
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
                                    NodeVector      &args)
{
    return acceptSubroutineCall(name, loc, args, false);
}

Node TypeCheck::acceptQualifiedProcedureCall(Node            qualNode,
                                             IdentifierInfo *name,
                                             Location        loc,
                                             NodeVector     &args)
{
    Qualifier         *qualifier = cast_node<Qualifier>(qualNode);
    DeclarativeRegion *region    = qualifier->resolve();
    std::vector<SubroutineDecl*> decls;

    // FIXME: Report that there are no functions of the given arity declared in
    // this domain.
    if (!region->collectProcedureDecls(name, args.size(), decls)) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    return acceptSubroutineCall(decls, loc, args);
}

Node TypeCheck::acceptReturnStmt(Location loc, Node retNode)
{
    assert((checkingProcedure() || checkingFunction()) &&
           "Return statement outside subroutine context!");

    if (checkingFunction()) {
        FunctionDecl *fdecl      = getCurrentFunction();
        Expr         *retExpr    = cast_node<Expr>(retNode);
        Type         *targetType = fdecl->getReturnType();

        if (checkType(retExpr, targetType)) {
            retNode.release();
            return getNode(new ReturnStmt(loc, retExpr));
        }
        return getInvalidNode();
    }

    report(loc, diag::NONEMPTY_RETURN_IN_PROCEDURE);
    return getInvalidNode();
}

Node TypeCheck::acceptEmptyReturnStmt(Location loc)
{
    assert((checkingProcedure() || checkingFunction()) &&
           "Return statement outside subroutine context!");

    if (checkingProcedure())
        return getNode(new ReturnStmt(loc));

    report(loc, diag::EMPTY_RETURN_IN_FUNCTION);
    return getInvalidNode();
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
        return getInvalidNode();
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
        return getInvalidNode();
    }

    if (checkType(value, targetDecl->getType())) {
        valueNode.release();
        DeclRefExpr *ref = new DeclRefExpr(targetDecl, loc);
        return getNode(new AssignmentStmt(ref, value));
    }

    return getInvalidNode();
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

        for (unsigned i = 0; i < numConsequents; ++i) {
            consequents->addStmt(cast_node<Stmt>(consequentNodes[i]));
            consequentNodes[i].release();
        }
        conditionNode.release();
        return getNode(new IfStmt(loc, condition, consequents));
    }

    report(condition->getLocation(), diag::INCOMPATIBLE_TYPES);
    return getInvalidNode();
}

Node TypeCheck::acceptElseStmt(Location loc, Node ifNode,
                               Node *alternateNodes, unsigned numAlternates)
{
    IfStmt       *cond       = cast_node<IfStmt>(ifNode);
    StmtSequence *alternates = new StmtSequence();

    assert(!cond->hasAlternate() && "Multiple else component in IfStmt!");

    for (unsigned i = 0; i < numAlternates; ++i) {
        alternates->addStmt(cast_node<Stmt>(alternateNodes[i]));
        alternateNodes[i].release();
    }

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
        for (unsigned i = 0; i < numConsequents; ++i) {
            consequents->addStmt(cast_node<Stmt>(consequentNodes[i]));
            consequentNodes[i].release();
        }
        conditionNode.release();
        cond->addElsif(loc, condition, consequents);
        return ifNode;
    }

    report(condition->getLocation(), diag::INCOMPATIBLE_TYPES);
    return getInvalidNode();
}

// Called when a block statement is about to be parsed.
Node TypeCheck::beginBlockStmt(Location loc, IdentifierInfo *label)
{
    // Create a new block node, establish a new declarative region and scope.
    DeclarativeRegion *region = currentDeclarativeRegion();
    BlockStmt         *block  = new BlockStmt(loc, region, label);

    declarativeRegion = block;
    scope.push();
    return getNode(block);
}

// This method is called for each statement associated with the block.
void TypeCheck::acceptBlockStmt(Node blockNode, Node stmtNode)
{
    BlockStmt *block = cast_node<BlockStmt>(blockNode);
    Stmt      *stmt  = cast_node<Stmt>(stmtNode);
    block->addStmt(stmt);
    stmtNode.release();
}

// Once the last statement of a block has been parsed, this method is called
// to inform the client that we are leaving the block context established by
// the last call to beginBlockStmt.
void TypeCheck::endBlockStmt(Node blockNode)
{
    declarativeRegion = currentDeclarativeRegion()->getParent();
    scope.pop();
}
