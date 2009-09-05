//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DeclProducer.h"
#include "Scope.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"
#include "comma/typecheck/TypeCheck.h"

#include "llvm/ADT/STLExtras.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

Node TypeCheck::acceptProcedureName(IdentifierInfo  *name,
                                    Location         loc,
                                    Node             qualNode)
{
    if (!qualNode.isNull()) {
        Qualifier  *qualifier = cast_node<Qualifier>(qualNode);
        DeclRegion *region    = qualifier->resolve();

        // Collect all of the function declarations in the region with the given
        // name.  If the name does not resolve uniquely, return an
        // OverloadedDeclName, otherwise the decl itself.
        typedef DeclRegion::PredRange PredRange;
        typedef DeclRegion::PredIter  PredIter;
        PredRange range = region->findDecls(name);
        llvm::SmallVector<SubroutineDecl*, 8> decls;

        // Collect all procedure decls.
        for (PredIter iter = range.first; iter != range.second; ++iter) {
            ProcedureDecl *candidate = dyn_cast<ProcedureDecl>(*iter);
            if (candidate)
                decls.push_back(candidate);
        }

        if (decls.empty()) {
            report(loc, diag::NAME_NOT_VISIBLE) << name;
            return getInvalidNode();
        }

        if (decls.size() == 1)
            return getNode(decls.front());

        return getNode(new OverloadedDeclName(&decls[0], decls.size()));
    }

    llvm::SmallVector<SubroutineDecl*, 8> overloads;
    Scope::Resolver &resolver = scope->getResolver();

    resolver.resolve(name);
    resolver.filterFunctionals();
    resolver.getVisibleSubroutines(overloads);

    unsigned numOverloads = overloads.size();
    if (numOverloads == 1)
        return getNode(overloads.front());
    else if (numOverloads > 1)
        return getNode(new OverloadedDeclName(&overloads[0], numOverloads));

    // Otherwise, we cannot resolve the name.
    report(loc, diag::NAME_NOT_VISIBLE) << name;
    return getInvalidNode();
}

Node TypeCheck::acceptProcedureCall(Node        connective,
                                    Location    loc,
                                    NodeVector &args)
{
    std::vector<SubroutineDecl*> decls;
    unsigned targetArity = args.size();

    connective.release();

    if (ProcedureDecl *pdecl = lift_node<ProcedureDecl>(connective)) {
        if (pdecl->getArity() == targetArity)
            decls.push_back(pdecl);
        else {
            report(loc, diag::WRONG_NUM_ARGS_FOR_SUBROUTINE)
                << pdecl->getIdInfo();
            return getInvalidNode();
        }
    }
    else {
        OverloadedDeclName *odn = cast_node<OverloadedDeclName>(connective);
        for (OverloadedDeclName::iterator iter = odn->begin();
             iter != odn->end(); ++iter) {
            if (ProcedureDecl *pdecl = dyn_cast<ProcedureDecl>(*iter)) {
                if (pdecl->getArity() == targetArity)
                    decls.push_back(pdecl);
            }
        }

        delete odn;

        // FIXME: Report that there are no procedures visible with the required
        // arity.
        if (decls.empty()) {
            report(loc, diag::NAME_NOT_VISIBLE) << odn->getIdInfo();
            return getInvalidNode();
        }
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

Node TypeCheck::acceptAssignmentStmt(Location        loc,
                                     IdentifierInfo *name,
                                     Node            valueNode)
{
    Expr *value = cast_node<Expr>(valueNode);
    Scope::Resolver &resolver = scope->getResolver();

    if (!resolver.resolve(name)) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    // FIXME: Only direct (lexical) values can be assigned to for now.  Revisit
    // the issue of assignment ot an exported name later.
    if (!resolver.hasDirectValue()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    ValueDecl *targetDecl = resolver.getDirectValue();

    // If the target decl is a parameter, ensure that it is not of mode "in".
    if (ParamValueDecl *param = dyn_cast<ParamValueDecl>(targetDecl)) {
        if (param->getParameterMode() == PM::MODE_IN) {
            report(loc, diag::ASSIGNMENT_TO_MODE_IN) << name;
            return getInvalidNode();
        }
    }

    if (checkExprInContext(value, targetDecl->getType())) {
        valueNode.release();
        DeclRefExpr *ref = new DeclRefExpr(targetDecl, loc);
        return getNode(new AssignmentStmt(ref, value));
    }

    return getInvalidNode();
}

Node TypeCheck::acceptIfStmt(Location loc, Node conditionNode,
                             NodeVector &consequentNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    Expr *condition = cast_node<Expr>(conditionNode);

    if (checkExprInContext(condition, declProducer->getBoolType())) {
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

    if (checkExprInContext(condition, declProducer->getBoolType())) {
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
    scope->push();
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
    scope->pop();
}

Node TypeCheck::acceptWhileStmt(Location loc, Node conditionNode,
                                NodeVector &stmtNodes)
{
    typedef NodeCaster<Stmt> caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, caster> iterator;

    Expr *condition = cast_node<Expr>(conditionNode);

    if (!checkExprInContext(condition, declProducer->getBoolType()))
        return getInvalidNode();

    iterator I(stmtNodes.begin(), caster());
    iterator E(stmtNodes.end(), caster());
    StmtSequence *body = new StmtSequence(I, E);

    conditionNode.release();
    stmtNodes.release();
    return getNode(new WhileStmt(loc, condition, body));
}
