//===-- codegen/DependencySet.cpp ----------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenRoutine.h"
#include "DependencySet.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/AggExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/ExprVisitor.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/StmtVisitor.h"

#include <algorithm>

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

namespace {

/// Implementation class for DependencySet.  This is a visitor which walks
/// statements and expressions, filling in the given llvm::UniqueVector with
/// each external capsule referenced.
class DependencyScanner : private StmtVisitor, private ExprVisitor {

    llvm::UniqueVector<const DomainInstanceDecl*> &dependents;

public:
    DependencyScanner(llvm::UniqueVector<const DomainInstanceDecl*> &dependents)
        : dependents(dependents) { }

    void scan(Stmt *stmt) { visitStmt(stmt); }

private:
    /// \name Statement vistors.
    //@{
    void visitStmtSequence(StmtSequence *node);
    void visitBlockStmt(BlockStmt *node);
    void visitProcedureCallStmt(ProcedureCallStmt *node);
    void visitReturnStmt(ReturnStmt *node);
    void visitAssignmentStmt(AssignmentStmt *node);
    void visitIfStmt(IfStmt *node);
    void visitWhileStmt(WhileStmt *node);
    void visitLoopStmt(LoopStmt *node);
    void visitRaiseStmt(RaiseStmt *node);
    void visitForStmt(ForStmt *node);
    //@}

    /// \name Expression visitors.
    //@{
    void visitFunctionCallExpr(FunctionCallExpr *node);
    void visitInjExpr(InjExpr *node);
    void visitPrjExpr(PrjExpr *node);
    void visitAggregateExpr(AggregateExpr *node);
    void visitQualifiedExpr(QualifiedExpr *node);
    void visitDereferenceExpr(DereferenceExpr *node);
    void visitAllocatorExpr(AllocatorExpr *node);
    void visitLengthAE(LengthAE *node);
    //@}

    void addDependents(const DomainInstanceDecl *instance);
};

} // end anonymous namespace.


//===----------------------------------------------------------------------===//
// DependecyScanner methods.

void DependencyScanner::addDependents(const DomainInstanceDecl *instance)
{
    // If the given instance is parameterized, insert each argument as a
    // dependency, ignoring abstract domains and % (the formal parameters of a
    // functor, nor the domain itself, need recording).
    if (instance->isParameterized()) {
        typedef DomainInstanceDecl::arg_iterator iterator;
        iterator E = instance->endArguments();
        for (iterator I = instance->beginArguments(); I != E; ++I) {
            DomainType *argTy = (*I)->getType();
            if (!(argTy->isAbstract() || argTy->denotesPercent())) {
                DomainInstanceDecl *argInstance = argTy->getInstanceDecl();
                assert(argInstance && "Bad domain type!");
                dependents.insert(argInstance);
            }
        }
    }
    dependents.insert(instance);
}

void DependencyScanner::visitStmtSequence(StmtSequence *node)
{
    for (StmtSequence::stmt_iter I = node->stmt_begin();
         I != node->stmt_end(); ++I)
        visitStmt(*I);

    if (node->isHandled()) {
        for (StmtSequence::handler_iter I = node->handler_begin();
             I != node->handler_end(); ++I)
            visitStmtSequence(*I);
    }
}

void DependencyScanner::visitBlockStmt(BlockStmt *node)
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

void DependencyScanner::visitProcedureCallStmt(ProcedureCallStmt *node)
{
    /// Add the connective as a dependency iff the call is direct.
    if (node->isDirectCall()) {
        ProcedureDecl *proc = node->getConnective();
        DomainInstanceDecl *instance =
            cast<DomainInstanceDecl>(proc->getDeclRegion());
        addDependents(instance);
    }

    typedef ProcedureCallStmt::arg_iterator iterator;
    iterator I = node->begin_arguments();
    iterator E = node->end_arguments();
    for ( ; I != E; ++I)
        visitExpr(*I);
}

void DependencyScanner::visitReturnStmt(ReturnStmt *node)
{
    if (node->hasReturnExpr())
        visitExpr(node->getReturnExpr());
}

void DependencyScanner::visitAssignmentStmt(AssignmentStmt *node)
{
    // The target of an assignment is always local to the current capsule, so
    // there are never any dependents.  Scan that rhs.
    visitExpr(node->getAssignedExpr());
}

void DependencyScanner::visitIfStmt(IfStmt *node)
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

void DependencyScanner::visitWhileStmt(WhileStmt *node)
{
    visitExpr(node->getCondition());
    visitStmtSequence(node->getBody());
}

void DependencyScanner::visitLoopStmt(LoopStmt *node)
{
    visitStmtSequence(node->getBody());
}

void DependencyScanner::visitRaiseStmt(RaiseStmt *node)
{
    if (node->hasMessage())
        visitExpr(node->getMessage());

    ExceptionDecl *exception;
    DeclRegion *region;
    DomainInstanceDecl *instance;
    exception = node->getExceptionDecl();
    region = exception->getDeclRegion();
    if (region && (instance = dyn_cast<DomainInstanceDecl>(region)))
        addDependents(instance);
}

void DependencyScanner::visitForStmt(ForStmt *node)
{
    DSTDefinition *control = node->getControl();

    if (control->definedUsingRange()) {
        Range *range = control->getRange();
        visitExpr(range->getLowerBound());
        visitExpr(range->getUpperBound());
    }
    else if (control->definedUsingAttrib()) {
        RangeAttrib *attrib = control->getAttrib();
        if (ArrayRangeAttrib *ARA = dyn_cast<ArrayRangeAttrib>(attrib))
            visitExpr(ARA->getPrefix());
    }
    else if (control->definedUsingConstraint()) {
        Range *range = control->getType()->getConstraint();
        visitExpr(range->getLowerBound());
        visitExpr(range->getUpperBound());
    }

    visitStmtSequence(node->getBody());
}

void DependencyScanner::visitFunctionCallExpr(FunctionCallExpr *node)
{
    /// Add the connective as a dependency iff the call is direct.
    if (node->isDirectCall()) {
        FunctionDecl *fn = node->getConnective(0);
        DomainInstanceDecl *instance =
            cast<DomainInstanceDecl>(fn->getDeclRegion());
        addDependents(instance);
    }

    typedef FunctionCallExpr::arg_iterator iterator;
    iterator I = node->begin_arguments();
    iterator E = node->end_arguments();
    for ( ; I != E; ++I)
        visitExpr(*I);
}

void DependencyScanner::visitInjExpr(InjExpr *node)
{
    visitExpr(node->getOperand());
}

void DependencyScanner::visitPrjExpr(PrjExpr *node)
{
    visitExpr(node->getOperand());
}

void DependencyScanner::visitAggregateExpr(AggregateExpr *node)
{
    typedef AggregateExpr::pos_iterator pos_iterator;
    for (pos_iterator I = node->pos_begin(), E = node->pos_end(); I != E; ++I)
        visitExpr(*I);

    typedef AggregateExpr::kl_iterator kl_iterator;
    for (kl_iterator I = node->kl_begin(), E = node->kl_end(); I != E; ++I)
        visitExpr((*I)->getExpr());

    if (Expr *others = node->getOthersExpr())
        visitExpr(others);
}

void DependencyScanner::visitQualifiedExpr(QualifiedExpr *node)
{
    visitExpr(node->getOperand());
}

void DependencyScanner::visitDereferenceExpr(DereferenceExpr *node)
{
    visitExpr(node->getPrefix());
}

void DependencyScanner::visitAllocatorExpr(AllocatorExpr *node)
{
    if (node->isInitialized())
        visitExpr(node->getInitializer());
}

void DependencyScanner::visitLengthAE(LengthAE *node)
{
    if (Expr *prefix = node->getPrefixExpr())
        visitExpr(prefix);
}

//===----------------------------------------------------------------------===//
// DependencySet methods.

void DependencySet::scan()
{
    typedef DeclRegion::ConstDeclIter decl_iterator;

    DependencyScanner DS(dependents);
    const AddDecl *add = capsule->getImplementation();

    // If there is no body, there is nothing to do.
    if (!add)
        return;

    // Iterate over each subroutine declaration present in the capsules body an
    // analyze each in turn.
    decl_iterator E = add->endDecls();
    for (decl_iterator I = add->beginDecls(); I != E; ++I) {
        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*I)) {
            if (srDecl->hasBody())
                DS.scan(srDecl->getBody());
        }
    }
}

DependencySet::iterator
DependencySet::find(const DomainInstanceDecl *instance) const
{
    return std::find(begin(), end(), instance);
}
