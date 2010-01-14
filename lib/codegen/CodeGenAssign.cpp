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

    /// Various emitter helpers conditional on the type of the lhs.
    void emitAssignment(DeclRefExpr *lhs, Expr *rhs);
    void emitAssignment(SelectedExpr *lhs, Expr *rhs);
    void emitAssignment(DereferenceExpr *lhs, Expr *rhs);
    void emitAssignment(IndexedArrayExpr *lhs, Expr *rhs);
};

} // end anonymous namespace.

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
    // the storage provided by the lhs.  It is a fat access type then store into
    // the data pointer.  Otherwise, simply store the lhs.
    llvm::Value *target = CGR.emitSelectedRef(lhs).first();
    PrimaryType *targetTy = CGR.resolveType(lhs->getType());
    if (targetTy->isCompositeType())
        CGR.emitCompositeExpr(rhs, target, false);
    else if (targetTy->isFatAccessType()) {
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(Builder.CreateLoad(source), target);
    }
    else {
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(source, target);
    }
}

void AssignmentEmitter::emitAssignment(DereferenceExpr *lhs, Expr *rhs)
{
    AccessType *prefixTy = lhs->getPrefixType();
    llvm::Value *target = CGR.emitValue(lhs->getPrefix()).first();

    // If the prefix is a fat access type extract the first component (the
    // pointer to the data).
    if (prefixTy->isFatAccessType()) {
        target = Builder.CreateStructGEP(target, 0);
        target = Builder.CreateLoad(target);
    }

    // Check that the target is not null and emit the rhs.
    PrimaryType *targetTy = CGR.resolveType(lhs->getType());
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
    CValue ptr = CGR.emitIndexedArrayRef(lhs);

    if (ptr.isSimple()) {
        llvm::Value *target = ptr.first();
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(source, target);
    }
    else if (ptr.isAggregate()) {
        llvm::Value *target = ptr.first();
        CGR.emitCompositeExpr(rhs, target, false);
    }
    else {
        assert(ptr.isFat());
        llvm::Value *target = ptr.first();
        llvm::Value *source = CGR.emitValue(rhs).first();
        Builder.CreateStore(Builder.CreateLoad(source), target);
    }
}

void AssignmentEmitter::emit(AssignmentStmt *stmt)
{
    Expr *lhs = stmt->getTarget();
    Expr *rhs = stmt->getAssignedExpr();

    // Remove any layers of inj/prj expressions from the left hand side.
    lhs = lhs->ignoreInjPrj();

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

