//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "TypeCheck.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/STLExtras.h"

using namespace comma;
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
        Expr *retExpr = cast_node<Expr>(retNode);
        Type *targetType = fdecl->getReturnType();

        if (checkExprInContext(retExpr, targetType)) {
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
    Expr *value = cast_node<Expr>(valueNode);
    Expr *target = 0;

    if (DeclRefExpr *ref = lift_node<DeclRefExpr>(targetNode)) {
        ValueDecl *targetDecl = ref->getDeclaration();

        // If the target decl is a parameter, ensure that it is not of mode
        // "in".
        if (ParamValueDecl *param = dyn_cast<ParamValueDecl>(targetDecl)) {
            if (param->getParameterMode() == PM::MODE_IN) {
                Location loc = ref->getLocation();
                IdentifierInfo *name = param->getIdInfo();
                report(loc, diag::ASSIGNMENT_TO_MODE_IN) << name;
                return getInvalidNode();
            }
        }
        target = ref;
    }
    else if (IndexedArrayExpr *idx = lift_node<IndexedArrayExpr>(targetNode)) {
        DeclRefExpr *arrayRef = idx->getArrayExpr();
        ValueDecl *arrayDecl = arrayRef->getDeclaration();

        // Again, ensure that if the array is a formal parameter, that it is not
        // of mode "in".
        if (ParamValueDecl *param = dyn_cast<ParamValueDecl>(arrayDecl)) {
            if (param->getParameterMode() == PM::MODE_IN) {
                Location loc = idx->getLocation();
                IdentifierInfo *name = param->getIdInfo();
                report(loc, diag::ASSIGNMENT_TO_MODE_IN) << name;
                return getInvalidNode();
            }
        }
        target = idx;
    }
    else {
        report(getNodeLoc(targetNode), diag::INVALID_LHS_FOR_ASSIGNMENT);
        return getInvalidNode();
    }

    // Check that the value is compatable with the type of the target.
    if (!checkExprInContext(value, target->getType()))
        return getInvalidNode();

    valueNode.release();
    targetNode.release();

    if (conversionRequired(value->getType(), target->getType()))
        value = new ConversionExpr(value, target->getType());
    return getNode(new AssignmentStmt(target, value));
}

Node TypeCheck::acceptIfStmt(Location loc, Node conditionNode,
                             NodeVector &consequentNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    Expr *condition = cast_node<Expr>(conditionNode);

    if (checkExprInContext(condition, resource.getTheBooleanType())) {
        iterator I(consequentNodes.begin(), caster());
        iterator E(consequentNodes.end(), caster());
        StmtSequence *consequents = new StmtSequence(I, E);

        conditionNode.release();
        consequentNodes.release();
        return getNode(new IfStmt(loc, condition, consequents));
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
    StmtSequence *alternates = new StmtSequence(I, E);

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
    Expr *condition = cast_node<Expr>(conditionNode);

    if (checkExprInContext(condition, resource.getTheBooleanType())) {
        iterator I(consequentNodes.begin(), caster());
        iterator E(consequentNodes.end(), caster());
        StmtSequence *consequents = new StmtSequence(I, E);

        cond->addElsif(loc, condition, consequents);
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

Node TypeCheck::acceptWhileStmt(Location loc, Node conditionNode,
                                NodeVector &stmtNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    Expr *condition = cast_node<Expr>(conditionNode);

    if (!checkExprInContext(condition, resource.getTheBooleanType()))
        return getInvalidNode();

    iterator I(stmtNodes.begin(), caster());
    iterator E(stmtNodes.end(), caster());
    StmtSequence *body = new StmtSequence(I, E);

    conditionNode.release();
    stmtNodes.release();
    return getNode(new WhileStmt(loc, condition, body));
}

Node TypeCheck::beginForStmt(Location loc,
                             IdentifierInfo *iterName, Location iterLoc,
                             Node control, bool isReversed)
{
    // FIXME: Only range attributes are currently supported as loop control.
    RangeAttrib *attrib = lift_node<RangeAttrib>(control);

    if (!attrib) {
        report(getNodeLoc(control), diag::INVALID_FOR_LOOP_CONTROL);
        return getInvalidNode();
    }

    // FIXME: Generalize to scalar types.
    DiscreteType *iterTy = attrib->getType();
    LoopDecl *iter = new LoopDecl(iterName, iterTy, iterLoc);
    ForStmt *loop = new ForStmt(loc, iter, attrib);

    if (isReversed)
        loop->markAsReversed();

    // Push a scope for the for loop and then add the loop parameter.
    scope.push();
    scope.addDirectDecl(iter);
    control.release();
    return getNode(loop);
}

Node TypeCheck::endForStmt(Node forNode, NodeVector &bodyNodes)
{
    // Pop the scope we entered for this loop.
    scope.pop();

    // The parser _always_ gives us the node we provided in the call to
    // beginForStmt.  This is one of only times when the parser might pass us an
    // invalid node.
    if (forNode.isInvalid())
        return getInvalidNode();

    // It is possible that the body is empty due to parse/semantic errors.  Do
    // not construct empty for loops.
    if (bodyNodes.empty())
        return getInvalidNode();

    // There is nothing to do but embed the body statements into the for loop.
    bodyNodes.release();
    ForStmt *loop = cast_node<ForStmt>(forNode);
    StmtSequence *body = loop->getBody();

    NodeVector::iterator I = bodyNodes.begin();
    NodeVector::iterator E = bodyNodes.end();
    for ( ; I != E; ++I) {
        Stmt *S = cast_node<Stmt>(*I);
        body->addStmt(S);
    }

    // Just reuse the given forNode as it now references the updated AST.
    return forNode;
}

Node TypeCheck::acceptPragmaStmt(IdentifierInfo *name, Location loc,
                                 NodeVector &argNodes)
{
    Pragma *pragma = 0;

    // The only pragma we currently support is "Assert".
    if (name == resource.getIdentifierInfo("Assert"))
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
