//===-- codegen/CodeGenGeneric.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenGeneric.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/ExprVisitor.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/StmtVisitor.h"
#include "comma/codegen/CodeGenRoutine.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

namespace {

/// Walks satements and expressions, calling
/// CodeGenCapsule::addCapsuleDependancy for each external capsule referenced.
class DependentScanner : private StmtVisitor, private ExprVisitor {

public:
    DependentScanner(CodeGenCapsule &CGC) : CGC(CGC) { }

    void scan(Stmt *stmt) { visitStmt(stmt); }

private:
    CodeGenCapsule &CGC;

    /// \name Statement vistors.
    ///@{
    void visitStmtSequence(StmtSequence *node);
    void visitBlockStmt(BlockStmt *node);
    void visitProcedureCallStmt(ProcedureCallStmt *node);
    void visitReturnStmt(ReturnStmt *node);
    void visitAssignmentStmt(AssignmentStmt *node);
    void visitIfStmt(IfStmt *node);
    void visitWhileStmt(WhileStmt *node);
    ///@}

    /// \name Expression visitors.
    void visitKeywordSelector(KeywordSelector *node);
    void visitFunctionCallExpr(FunctionCallExpr *node);
    void visitInjExpr(InjExpr *node);
    void visitPrjExpr(PrjExpr *node);
    ///@}
};

} // end anonymous namespace.


//===----------------------------------------------------------------------===//
// DependentScanner methods.

void DependentScanner::visitStmtSequence(StmtSequence *node)
{
    typedef StmtSequence::StmtIter iterator;
    iterator E = node->endStatements();
    for (iterator I = node->beginStatements(); I != E; ++I)
        visitStmt(*I);
}

void DependentScanner::visitBlockStmt(BlockStmt *node)
{
    // Scan the block statement for object declarations with initializers.
    typedef BlockStmt::DeclIter iterator;
    iterator E = node->endDecls();
    for (iterator I = node->beginDecls(); I != E; ++I) {
        if (ObjectDecl *decl = dyn_cast<ObjectDecl>(*I)) {
            if (decl->hasInitializer())
                visitExpr(decl->getInitializer());
        }
    }

    // Scan the associated sequence of statements.
    visitStmtSequence(node);
}

void DependentScanner::visitProcedureCallStmt(ProcedureCallStmt *node)
{
    /// Add the connective as a dependency iff the call is direct.
    if (CodeGenRoutine::isDirectCall(node)) {
        ProcedureDecl *proc = node->getConnective();
        DomainInstanceDecl *instance =
            cast<DomainInstanceDecl>(proc->getDeclRegion());
        CGC.addCapsuleDependency(instance);
    }

    typedef ProcedureCallStmt::arg_iterator iterator;
    iterator I = node->begin_arguments();
    iterator E = node->end_arguments();
    for ( ; I != E; ++I)
        visitExpr(*I);
}

void DependentScanner::visitReturnStmt(ReturnStmt *node)
{
    if (node->hasReturnExpr())
        visitExpr(node->getReturnExpr());
}

void DependentScanner::visitAssignmentStmt(AssignmentStmt *node)
{
    // The target of an assignment is always local to the current capsule, so
    // there are never any dependents.  Scan that rhs.
    visitExpr(node->getAssignedExpr());
}

void DependentScanner::visitIfStmt(IfStmt *node)
{
    visitExpr(node->getCondition());
    visitStmtSequence(node->getConsequent());

    IfStmt::iterator E = node->endElsif();
    for (IfStmt::iterator I = node->beginElsif(); I != E; ++I) {
        visitExpr(I->getCondition());
        visitStmtSequence(I->getConsequent());
    }

    if (node->hasAlternate())
        visitStmtSequence(node->getAlternate());
}

void DependentScanner::visitWhileStmt(WhileStmt *node)
{
    visitExpr(node->getCondition());
    visitStmtSequence(node->getBody());
}

void DependentScanner::visitKeywordSelector(KeywordSelector *node)
{
    visitExpr(node->getExpression());
}

void DependentScanner::visitFunctionCallExpr(FunctionCallExpr *node)
{
    /// Add the connective as a dependency iff the call is direct.
    if (CodeGenRoutine::isDirectCall(node)) {
        FunctionDecl *fn = node->getConnective(0);
        DomainInstanceDecl *instance =
            cast<DomainInstanceDecl>(fn->getDeclRegion());
        CGC.addCapsuleDependency(instance);
    }

    typedef FunctionCallExpr::arg_iterator iterator;
    iterator I = node->begin_arguments();
    iterator E = node->end_arguments();
    for ( ; I != E; ++I)
        visitExpr(*I);
}

void DependentScanner::visitInjExpr(InjExpr *node)
{
    visitExpr(node->getOperand());
}

void DependentScanner::visitPrjExpr(PrjExpr *node)
{
    visitExpr(node->getOperand());
}

//===----------------------------------------------------------------------===//
// CodeGenGeneric methods.

CodeGenGeneric::CodeGenGeneric(CodeGenCapsule &CGC)
    : CGC(CGC),
      capsule(dyn_cast_or_null<FunctorDecl>(CGC.getCapsule()))
{
    assert(capsule && "Expected a generic capsule!");
}

/// Scans the associated capsule for references to external domains.
///
/// For each external capsule found, CodeGenCapsule::addCapsuleDependency is
/// called to register the external reference.
void CodeGenGeneric::analyzeDependants()
{
    DependentScanner DS(CGC);

    // Iterate over each subroutine declaration present in the capsules body and
    // analyze each in turn.
    if (const AddDecl *add = capsule->getImplementation()) {
        typedef DeclRegion::ConstDeclIter iterator;
        for (iterator iter = add->beginDecls();
             iter != add->endDecls(); ++iter) {
            if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*iter)) {
                if (!SR->hasBody())
                    continue;
                DS.scan(SR->getBody());
            }
        }
    }
}
