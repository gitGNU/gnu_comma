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
            report(loc, diag::NAME_NOT_VISIBLE);
            return getInvalidNode();
        }

        if (decls.size() == 1)
            return getNode(decls.front());

        return getNode(new OverloadedDeclName(&decls[0], decls.size()));
    }

    Scope::Resolver &resolver = scope->getResolver();

    if (!resolver.resolve(name) || resolver.hasDirectValue()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    llvm::SmallVector<SubroutineDecl *, 8> overloads;
    unsigned numOverloads = 0;
    resolver.filterFunctionals();

    // Collect any direct overloads.
    {
        typedef Scope::Resolver::direct_overload_iter iterator;
        iterator E = resolver.end_direct_overloads();
        for (iterator I = resolver.begin_direct_overloads(); I != E; ++I)
            overloads.push_back(cast<ProcedureDecl>(*I));
    }

    // Continue populating the call with indirect overloads if there are no
    // indirect values visible and return the result.
    if (!resolver.hasIndirectValues()) {
        typedef Scope::Resolver::indirect_overload_iter iterator;
        iterator E = resolver.end_indirect_overloads();
        for (iterator I = resolver.begin_indirect_overloads(); I != E; ++I)
            overloads.push_back(cast<ProcedureDecl>(*I));
        numOverloads = overloads.size();
        if (numOverloads == 1)
            return getNode(overloads.front());
        else if (numOverloads > 1)
            return getNode(new OverloadedDeclName(&overloads[0], numOverloads));
        else {
            report(loc, diag::NAME_NOT_VISIBLE) << name;
            return getInvalidNode();
        }
    }

    // There are indirect values which shadow all indirect procedures.  If we
    // have any direct decls, return a node for them.
    numOverloads = overloads.size();
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

Node TypeCheck::acceptIfStmt(Location loc,
                             Node     conditionNode,
                             Node    *consequentNodes,
                             unsigned numConsequents)
{
    Expr *condition = cast_node<Expr>(conditionNode);

    if (checkExprInContext(condition, declProducer->getBoolType())) {
        StmtSequence *consequents = new StmtSequence();

        for (unsigned i = 0; i < numConsequents; ++i) {
            consequents->addStmt(cast_node<Stmt>(consequentNodes[i]));
            consequentNodes[i].release();
        }
        conditionNode.release();
        return getNode(new IfStmt(loc, condition, consequents));
    }
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

    if (checkExprInContext(condition, declProducer->getBoolType())) {
        StmtSequence *consequents = new StmtSequence();
        for (unsigned i = 0; i < numConsequents; ++i) {
            consequents->addStmt(cast_node<Stmt>(consequentNodes[i]));
            consequentNodes[i].release();
        }
        conditionNode.release();
        cond->addElsif(loc, condition, consequents);
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
    Expr *condition = cast_node<Expr>(conditionNode);

    if (!checkExprInContext(condition, declProducer->getBoolType()))
        return getInvalidNode();

    StmtSequence *body = new StmtSequence();
    for (NodeVector::iterator I = stmtNodes.begin();
         I != stmtNodes.end(); ++I) {
        Stmt *stmt = cast_node<Stmt>(*I);
        body->addStmt(stmt);
    }
    conditionNode.release();
    stmtNodes.release();
    return getNode(new WhileStmt(loc, condition, body));
}
