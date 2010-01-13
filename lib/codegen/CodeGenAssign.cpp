//===-- codegen/CodeGenAssign.cpp ----------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file contains the codegen routines responsible for the
/// synthesization of assignment statements.
//===----------------------------------------------------------------------===//

#include "CodeGenRoutine.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

class AssignmentEmitter {

public:
    AssignmentEmitter(CodeGenRoutine &CGR)
        : CGR(CGR), Builder(CGR.getSRFrame()->getIRBuilder()) { }

    /// Emits the given assignment statement.
    void emit(AssignmentStmt *assignment);

private:
    /// Codegen context.
    CodeGenRoutine &CGR;

    /// Current IRBuilder.
    llvm::IRBuilder<> &Builder;

    /// Returns the currently active frame.
    SRFrame *frame() { return CGR.getSRFrame(); }

    /// Strips the given expression of any outer inj/prj expressions, returning
    /// the innermost operand (or the given expression if it is not an inj or
    /// prj).
    static Expr *stripInjPrj(Expr *expr);

    /// Various emitter helpers conditional on the type of the lhs.
    void emitAssignment(DeclRefExpr *lhs, Expr *rhs);
    void emitAssignment(SelectedExpr *lhs, Expr *rhs);
    void emitAssignment(DereferenceExpr *lhs, Expr *rhs);
    void emitAssignment(IndexedArrayExpr *lhs, Expr *rhs);
};

} // end anonymous namespace.

Expr *AssignmentEmitter::stripInjPrj(Expr *expr)
{
    for (;;) {
        switch (expr->getKind()) {

        default:
            return expr;

        case Ast::AST_InjExpr:
            expr = cast<InjExpr>(expr)->getOperand();
            break;

        case Ast::AST_PrjExpr:
            expr = cast<PrjExpr>(expr)->getOperand();
            break;
        }
    }
}

void AssignmentEmitter::emitAssignment(DeclRefExpr *lhs, Expr *rhs)
{
    Type *targetTy = CGR.resolveType(lhs->getType());
    ValueDecl *lhsDecl = lhs->getDeclaration();
    llvm::Value *target = frame()->lookup(lhsDecl, activation::Slot);

    if (targetTy->isCompositeType()) {
        // Evaluate the rhs into the storage provided by the lhs.
        CGR.emitCompositeExpr(rhs, target, false);
    }
    else {
        // The lhs is a simple variable reference.  Just emit and store.
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(source, target);
    }
}

void AssignmentEmitter::emitAssignment(SelectedExpr *lhs, Expr *rhs)
{
    // If the type of the selected component is composite evaluate the rhs into
    // the storage provided by the lhs.  Otherwise, simply store the lhs.
    llvm::Value *target = CGR.emitSelectedRef(lhs);
    PrimaryType *targetTy = CGR.resolveType(lhs->getType());
    if (targetTy->isCompositeType())
        CGR.emitCompositeExpr(rhs, target, false);
    else {
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(source, target);
    }
}

void AssignmentEmitter::emitAssignment(DereferenceExpr *lhs, Expr *rhs)
{
    // Emit the prefix as a simple value yielding a pointer to the
    // destination.  Check that the target is not null and emit the rhs.
    llvm::Value *target = CGR.emitValue(lhs->getPrefix()).first();
    PrimaryType *targetTy = CGR.resolveType(lhs->getPrefixType());
    CGR.emitNullAccessCheck(target);
    if (targetTy->isCompositeType())
        CGR.emitCompositeExpr(rhs, target, false);
    else {
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(source, target);
    }
}

void AssignmentEmitter::emitAssignment(IndexedArrayExpr *lhs, Expr *rhs)
{
    // Get a reference to the needed component and store.
    llvm::Value *target = CGR.emitIndexedArrayRef(lhs);
    llvm::Value *source = CGR.emitValue(rhs).first();
    Builder.CreateStore(source, target);
}

void AssignmentEmitter::emit(AssignmentStmt *stmt)
{
    Expr *lhs = stmt->getTarget();
    Expr *rhs = stmt->getAssignedExpr();

    // Remove any layers of inj/prj expressions from the left hand side.
    lhs = stripInjPrj(lhs);

#define DISPATCH(KIND, LHS, RHS)              \
    Ast::AST_ ## KIND:                        \
        emitAssignment(cast<KIND>(LHS), RHS); \
        break

    switch (lhs->getKind()) {

    default:
        assert(false && "Cannot codegen assignment!");
        break;

    case DISPATCH(DeclRefExpr, lhs, rhs);
    case DISPATCH(SelectedExpr, lhs, rhs);
    case DISPATCH(DereferenceExpr, lhs, rhs);
    case DISPATCH(IndexedArrayExpr, lhs, rhs);
    }

#undef DISPATCH
}

//===----------------------------------------------------------------------===//
// Public API for this file.

void CodeGenRoutine::emitAssignmentStmt(AssignmentStmt *stmt)
{
    AssignmentEmitter emitter(*this);
    emitter.emit(stmt);
}
