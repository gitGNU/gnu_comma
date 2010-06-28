//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "RangeChecker.h"
#include "Scope.h"
#include "TypeCheck.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/ExceptionRef.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/STLExtras.h"

using namespace comma;
using llvm::dyn_cast_or_null;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

Node TypeCheck::acceptProcedureCall(Node name)
{
    // If the name denotes a procedure call, we are happy.
    if (lift_node<ProcedureCallStmt>(name)) {
        name.release();
        return name;
    }

    // Otherwise, figure out what kind of name this is and grab its location.
    Location loc;
    if (Expr *expr = lift_node<Expr>(name))
        loc = expr->getLocation();
    else if (TypeRef *ref = lift_node<TypeRef>(name))
        loc = ref->getLocation();

    report(loc, diag::EXPECTED_PROCEDURE_CALL);
    return getInvalidNode();
}

Node TypeCheck::acceptReturnStmt(Location loc, Node retNode)
{
    assert((checkingProcedure() || checkingFunction()) &&
           "Return statement outside subroutine context!");

    if (checkingFunction()) {
        FunctionDecl *fdecl = getCurrentFunction();
        Type *targetType = fdecl->getReturnType();
        Expr *retExpr = ensureExpr(retNode);

        if (!retExpr)
            return getInvalidNode();

        if ((retExpr = checkExprInContext(retExpr, targetType))) {
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

Node TypeCheck::acceptAssignmentStmt(Node targetNode, Node valueNode)
{
    Expr *value = ensureExpr(valueNode);
    Expr *target = ensureExpr(targetNode);
    Expr *immutable;

    if (!(value && target))
        return getInvalidNode();

    if (!target->isMutable(immutable)) {
        Location loc = target->getLocation();

        // Diagnose common assignment mistakes.
        if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(immutable)) {
            if (isa<LoopDecl>(ref->getDeclaration())) {
                report(loc, diag::LOOP_PARAM_NOT_VARIABLE);
                return getInvalidNode();
            }
        }

        // Generic diagnostic.
        report(loc, diag::INVALID_TARGET_FOR_ASSIGNMENT);
        return getInvalidNode();
    }

    // Check that the value is compatible with the type of the target.
    Type *targetTy = target->getType();
    if (!(value = checkExprInContext(value, targetTy)))
        return getInvalidNode();

    valueNode.release();
    targetNode.release();
    value = convertIfNeeded(value, targetTy);
    return getNode(new AssignmentStmt(target, value));
}

Node TypeCheck::acceptIfStmt(Location loc, Node conditionNode,
                             NodeVector &consequentNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    Expr *pred = cast_node<Expr>(conditionNode);

    if ((pred = checkExprInContext(pred, resource.getTheBooleanType()))) {
        iterator I(consequentNodes.begin(), caster());
        iterator E(consequentNodes.end(), caster());
        StmtSequence *consequents = new StmtSequence(loc, I, E);

        conditionNode.release();
        consequentNodes.release();
        return getNode(new IfStmt(loc, pred, consequents));
    }
    return getInvalidNode();
}

Node TypeCheck::acceptElseStmt(Location loc, Node ifNode,
                               NodeVector &alternateNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    IfStmt *cond = cast_node<IfStmt>(ifNode);
    assert(!cond->hasAlternate() && "Multiple else component in IfStmt!");

    iterator I(alternateNodes.begin(), caster());
    iterator E(alternateNodes.end(), caster());
    StmtSequence *alternates = new StmtSequence(loc, I, E);

    cond->setAlternate(loc, alternates);
    alternateNodes.release();
    return ifNode;
}

Node TypeCheck::acceptElsifStmt(Location loc, Node ifNode, Node conditionNode,
                                NodeVector &consequentNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    IfStmt *cond = cast_node<IfStmt>(ifNode);
    Expr *pred = cast_node<Expr>(conditionNode);

    if ((pred = checkExprInContext(pred, resource.getTheBooleanType()))) {
        iterator I(consequentNodes.begin(), caster());
        iterator E(consequentNodes.end(), caster());
        StmtSequence *consequents = new StmtSequence(loc, I, E);

        cond->addElsif(loc, pred, consequents);
        conditionNode.release();
        consequentNodes.release();
        return ifNode;
    }
    return getInvalidNode();
}

// Called when a block statement is about to be parsed.
Node TypeCheck::beginBlockStmt(Location loc, IdentifierInfo *label)
{
    // Create a new block node, establish a new declarative region and scope.
    DeclRegion *region = currentDeclarativeRegion();
    BlockStmt  *block  = new BlockStmt(loc, region, label);

    declarativeRegion = block;
    scope.push();
    return getNode(block);
}

// Once the last statement of a block has been parsed, this method is called
// to inform the client that we are leaving the block context established by
// the last call to beginBlockStmt.
void TypeCheck::endBlockStmt(Node blockNode)
{
    declarativeRegion = currentDeclarativeRegion()->getParent();
    scope.pop();
}

bool TypeCheck::acceptStmt(Node contextNode, Node stmtNode)
{
    Stmt *context = cast_node<Stmt>(contextNode);
    Stmt *stmt = cast_node<Stmt>(stmtNode);
    StmtSequence *seq;

    switch (context->getKind()) {

    default:
        assert(false && "Invalid context for acceptStmt!");
        return false;

    case Ast::AST_BlockStmt:
        seq = cast<BlockStmt>(context);
        break;

    case Ast::AST_ForStmt:
        seq = cast<ForStmt>(context)->getBody();
        break;

    case Ast::AST_WhileStmt:
        seq = cast<WhileStmt>(context)->getBody();
        break;

    case Ast::AST_LoopStmt:
        seq = cast<LoopStmt>(context)->getBody();
        break;

    case Ast::AST_HandlerStmt:
        seq = cast<HandlerStmt>(context);
        break;
    }

    // If this sequence of statements is non-empty ensure the next statement
    // does not follow a terminator.  Unreachable statements are not considered
    // an error, so we return true and let the node reclaim the unused
    // statement.
    if (!seq->empty()) {
        Stmt *predecessor = seq->back();
        if (predecessor->isTerminator()) {
            report(stmt->getLocation(), diag::UNREACHABLE_STATEMENT);
            return true;
        }
    }

    stmtNode.release();
    seq->addStmt(stmt);
    return true;
}

Node TypeCheck::beginWhileStmt(Location loc, Node conditionNode,
                               IdentifierInfo *tag, Location tagLoc)
{
    Expr *pred = ensureExpr(conditionNode);

    if (!pred)
        return getInvalidNode();

    conditionNode.release();
    WhileStmt *loop = new WhileStmt(loc, pred);

    if (tag)
        loop->setTag(tag, tagLoc);

    activeLoops.push_back(loop);
    return getNode(loop);
}

Node TypeCheck::endWhileStmt(Node whileNode)
{
    WhileStmt *loop = cast_node<WhileStmt>(whileNode);

    assert(loop == activeLoops.back() && "Loop stack imbalance!");
    activeLoops.pop_back();

    // It is possible that the body is empty due to parse/semantic errors.  Do
    // not propagate empty for loops.
    if (loop->getBody()->empty())
        return getInvalidNode();

    return whileNode;
}

Node TypeCheck::beginLoopStmt(Location loc,
                              IdentifierInfo *tag, Location tagLoc)
{
    LoopStmt *loop = new LoopStmt(loc);

    if (tag)
        loop->setTag(tag, tagLoc);

    activeLoops.push_back(loop);
    return getNode(loop);
}

Node TypeCheck::endLoopStmt(Node loopNode)
{
    LoopStmt *loop = cast_node<LoopStmt>(loopNode);

    assert(loop == activeLoops.back() && "Loop stack imbalance!");
    activeLoops.pop_back();

    // It is possible that the body is empty due to parse/semantic errors.  Do
    // not propagate empty for loops.
    if (loop->getBody()->empty())
        return getInvalidNode();

    return loopNode;
}

Node TypeCheck::beginForStmt(Location loc,
                             IdentifierInfo *iterName, Location iterLoc,
                             Node controlNode, bool isReversed,
                             IdentifierInfo *tag, Location tagLoc)
{
    DSTDefinition *control = cast_node<DSTDefinition>(controlNode);
    DiscreteType *iterTy = control->getType();
    LoopDecl *iter = new LoopDecl(iterName, iterTy, iterLoc);
    ForStmt *loop = new ForStmt(loc, iter, control);

    if (isReversed)
        loop->markAsReversed();

    if (tag)
        loop->setTag(tag, tagLoc);

    // Push a scope for the for loop and then add the loop parameter.
    scope.push();
    scope.addDirectDecl(iter);
    activeLoops.push_back(loop);
    controlNode.release();
    return getNode(loop);
}

Node TypeCheck::endForStmt(Node forNode)
{
    // Pop the scope we entered for this loop.
    scope.pop();

    ForStmt *loop = cast_node<ForStmt>(forNode);

    assert(loop == activeLoops.back() && "Loop stack imbalance!");
    activeLoops.pop_back();

    // It is possible that the body is empty due to parse/semantic errors.  Do
    // not propagate empty for loops.
    if (loop->getBody()->empty())
        return getInvalidNode();

    return forNode;
}

Node TypeCheck::acceptExitStmt(Location exitLoc,
                               IdentifierInfo *tag, Location tagLoc,
                               Node conditionNode)
{
    // Ensure that a loop is active.
    if (activeLoops.empty()) {
        report(exitLoc, diag::EXIT_OUTSIDE_LOOP_CONTEXT);
        return getInvalidNode();
    }

    // If a tag was given ensure that an identically tagged loop exists.
    if (tag) {
        bool found = false;
        ActiveLoopSet::iterator I = activeLoops.begin();
        ActiveLoopSet::iterator E = activeLoops.end();
        for ( ; I != E; ++I) {
            IterationStmt *loop = *I;
            if (loop->getTag() == tag) {
                found = true;
                break;
            }
        }

        if (!found) {
            report(tagLoc, diag::NONEXISTENT_LOOP_TAG) << tag;
            return getInvalidNode();
        }
    }

    // Check that the condition is boolean valued.
    Expr *condition = 0;
    if (!conditionNode.isNull()) {
        Type *theBoolean = resource.getTheBooleanType();
        condition = ensureExpr(conditionNode);

        if (!condition ||
            !(condition = checkExprInContext(condition, theBoolean)))
            return getInvalidNode();
    }

    ExitStmt *exit =
        tag ? new ExitStmt(exitLoc, tag, tagLoc) : new ExitStmt(exitLoc);
    conditionNode.release();
    if (condition)
        exit->setCondition(condition);
    return getNode(exit);
}

Node TypeCheck::acceptPragmaStmt(IdentifierInfo *name, Location loc,
                                 NodeVector &argNodes)
{
    Pragma *pragma = 0;

    // The only pragma we currently support is "Assert".
    if (name == resource.getIdentifierInfo("assert"))
        pragma = acceptPragmaAssert(loc, argNodes);
    else {
        // The parser knows all about pragmas, so we should always have a match.
        assert(pragma && "Unrecognized pragma!");
    }

    if (pragma) {
        argNodes.release();
        return getNode(new PragmaStmt(pragma));
    }
    else
        return getInvalidNode();
}

Node TypeCheck::acceptRaiseStmt(Location raiseLoc, Node exceptionNode,
                                Node messageNode)
{
    ExceptionRef *ref = lift_node<ExceptionRef>(exceptionNode);
    Expr *message = 0;

    if (!ref) {
        report(getNodeLoc(exceptionNode), diag::NOT_AN_EXCEPTION);
        return getInvalidNode();
    }

    if (!messageNode.isNull()) {
        Expr *expr = ensureExpr(messageNode);
        ArrayType *theStringType = resource.getTheStringType();
        if (!expr || !(expr = checkExprInContext(expr, theStringType)))
            return getInvalidNode();
        message = expr;
    }

    exceptionNode.release();
    messageNode.release();
    RaiseStmt *raise = new RaiseStmt(raiseLoc, ref, message);
    return getNode(raise);
}

Node TypeCheck::beginHandlerStmt(Location loc, NodeVector &choiceNodes)
{
    typedef NodeLifter<ExceptionRef> lifter;
    typedef llvm::mapped_iterator<NodeVector::iterator, lifter> iterator;

    typedef llvm::SmallVector<ExceptionRef*, 8> ChoiceVec;
    ChoiceVec choices;

    // Simply ensure that all choices resolve to ExceptionRef's.
    bool allOK = true;
    iterator I(choiceNodes.begin(), lifter());
    iterator E(choiceNodes.end(), lifter());
    for ( ; I != E; ++I) {
        if (ExceptionRef *ref = *I)
            choices.push_back(ref);
        else {
            report(getNodeLoc(*I.getCurrent()), diag::NOT_AN_EXCEPTION);
            allOK = false;
        }
    }
    if (!allOK)
        return getInvalidNode();

    choiceNodes.release();
    HandlerStmt *handler = new HandlerStmt(loc, choices.data(), choices.size());
    return getNode(handler);
}

void TypeCheck::endHandlerStmt(Node context, Node handlerNode)
{
    StmtSequence *handledSequence;
    HandlerStmt *handler = cast_node<HandlerStmt>(handlerNode);

    // The only valid context for handlers are block and subroutine
    // declarations.
    if (SubroutineDecl *SR = lift_node<SubroutineDecl>(context))
        handledSequence = SR->getBody();
    else
        handledSequence = cast_node<BlockStmt>(context);

    handlerNode.release();
    handledSequence->addHandler(handler);
}

Node TypeCheck::acceptNullStmt(Location loc)
{
    return getNode(new NullStmt(loc));
}
