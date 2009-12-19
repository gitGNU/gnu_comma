//===-- codege/CodeGenStmt.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void CodeGenRoutine::emitStmt(Stmt *stmt)
{
    switch (stmt->getKind()) {

    default:
        assert(false && "Cannot codegen stmt yet!");

    case Ast::AST_ProcedureCallStmt:
        emitProcedureCallStmt(cast<ProcedureCallStmt>(stmt));
        break;

    case Ast::AST_AssignmentStmt:
        emitAssignmentStmt(cast<AssignmentStmt>(stmt));
        break;

    case Ast::AST_StmtSequence:
        emitStmtSequence(cast<StmtSequence>(stmt));
        break;

    case Ast::AST_BlockStmt:
        emitBlockStmt(cast<BlockStmt>(stmt), Builder.GetInsertBlock());
        break;

    case Ast::AST_IfStmt:
        emitIfStmt(cast<IfStmt>(stmt));
        break;

    case Ast::AST_WhileStmt:
        emitWhileStmt(cast<WhileStmt>(stmt));
        break;

    case Ast::AST_ForStmt:
        emitForStmt(cast<ForStmt>(stmt));
        break;

    case Ast::AST_LoopStmt:
        emitLoopStmt(cast<LoopStmt>(stmt));
        break;

    case Ast::AST_ReturnStmt:
        emitReturnStmt(cast<ReturnStmt>(stmt));
        break;

    case Ast::AST_RaiseStmt:
        emitRaiseStmt(cast<RaiseStmt>(stmt));
        break;

    case Ast::AST_PragmaStmt:
        emitPragmaStmt(cast<PragmaStmt>(stmt));
        break;
    }
}

void CodeGenRoutine::emitReturnStmt(ReturnStmt *ret)
{
    if (!ret->hasReturnExpr()) {
        SRF->emitReturn();
        return;
    }

    FunctionDecl *fdecl = cast<FunctionDecl>(SRI->getDeclaration());
    FunctionType *fTy = fdecl->getType();
    Type *targetTy = resolveType(fTy->getReturnType());
    Expr *expr = ret->getReturnExpr();
    llvm::Value *returnValue = SRF->getReturnValue();

    if (ArrayType *arrTy = dyn_cast<ArrayType>(targetTy)) {
        if (arrTy->isConstrained()) {
            // The return slot corresponds to an sret return parameter.
            // Evaluate the return value into this slot.
            if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(expr))
                emitCompositeCall(call, returnValue);
            else
                emitArrayExpr(expr, returnValue, false);
        }
        else {
            // Unconstrained array types are returned via the vstack.
            assert(returnValue == 0);
            if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(expr)) {
                // Perform a "tail call" using the vstack.
                emitSimpleCall(call);
            }
            else {
                BoundsEmitter emitter(*this);
                std::pair<llvm::Value*, llvm::Value*> arrayPair =
                    emitArrayExpr(expr, 0, false);
                const llvm::Type *componentTy =
                    CGT.lowerType(arrTy->getComponentType());
                llvm::Value *data = arrayPair.first;
                llvm::Value *bounds = arrayPair.second;
                const llvm::Type *boundsTy = bounds->getType();

                // Compute the size of the data.  Note that getSizeOf returns an
                // i64 but we need an i32 here.
                llvm::Value *dataSize;
                dataSize = llvm::ConstantExpr::getSizeOf(componentTy);
                dataSize = Builder.CreateTrunc(dataSize, CG.getInt32Ty());
                dataSize = Builder.CreateMul(
                    dataSize, emitter.computeTotalBoundLength(Builder, bounds));

                // Compute the size of the bounds structure.  This is dependent
                // on if the bounds were given as a pointer or an aggregate.
                llvm::Value *boundsSize;
                if (boundsTy->isAggregateType())
                    boundsSize = llvm::ConstantExpr::getSizeOf(boundsTy);
                else {
                    const llvm::PointerType *ptr;
                    ptr = llvm::cast<llvm::PointerType>(boundsTy);
                    boundsSize = llvm::ConstantExpr::getSizeOf(ptr->getElementType());
                }
                boundsSize = Builder.CreateTrunc(boundsSize, CG.getInt32Ty());

                // Generate a temporary if needed so that we can obtain a
                // pointer to the bounds.
                if (boundsTy->isAggregateType()) {
                    llvm::Value *slot = SRF->createTemp(boundsTy);
                    Builder.CreateStore(bounds, slot);
                    bounds = slot;
                }

                CRT.vstack_push(Builder, data, dataSize);
                CRT.vstack_push(Builder, bounds, boundsSize);
            }
        }
    }
    else {
        // Non-composite return values can be simply evaluated and stored into
        // this subroutines return slot.
        llvm::Value *res = emitValue(ret->getReturnExpr());
        Builder.CreateStore(res, returnValue);
    }

    // Branch to the return block.
    SRF->emitReturn();
}

void CodeGenRoutine::emitStmtSequence(StmtSequence *seq)
{
    for (StmtSequence::stmt_iter iter = seq->stmt_begin();
         iter != seq->stmt_end(); ++iter)
        emitStmt(*iter);
}

llvm::BasicBlock *CodeGenRoutine::emitBlockStmt(BlockStmt *block,
                                                llvm::BasicBlock *predecessor)
{
    assert(block && "NULL block statement!");
    std::string label;

    if (block->hasLabel())
        label = block->getLabel()->getString();

    llvm::BasicBlock *BB = SRF->makeBasicBlock(label);

    if (predecessor) {
        Builder.CreateBr(BB);
        BB->moveAfter(predecessor);
    }

    Builder.SetInsertPoint(BB);

    // Generate any declarations provided by this block, followed by the blocks
    // sequence of statements.
    typedef DeclRegion::DeclIter iterator;
    for (iterator I = block->beginDecls(); I != block->endDecls(); ++I) {
        Decl *decl = *I;
        switch (decl->getKind()) {
        default:
            break;

        case Ast::AST_ObjectDecl:
            emitObjectDecl(cast<ObjectDecl>(decl));
            break;

        case Ast::AST_IntegerSubtypeDecl:
            emitIntegerSubtypeDecl(cast<IntegerSubtypeDecl>(decl));
            break;

        case Ast::AST_EnumSubtypeDecl:
            emitEnumSubtypeDecl(cast<EnumSubtypeDecl>(decl));
            break;
        }
    }
    emitStmtSequence(block);
    return BB;
}

void CodeGenRoutine::emitIfStmt(IfStmt *ite)
{
    llvm::Value *condition = emitValue(ite->getCondition());
    llvm::BasicBlock *thenBB = SRF->makeBasicBlock("then");
    llvm::BasicBlock *mergeBB = SRF->makeBasicBlock("merge");
    llvm::BasicBlock *elseBB;

    if (ite->hasElsif())
        elseBB = SRF->makeBasicBlock("elsif");
    else if (ite->hasAlternate())
        elseBB = SRF->makeBasicBlock("else");
    else
        elseBB = mergeBB;

    Builder.CreateCondBr(condition, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    emitStmt(ite->getConsequent());

    // If generation of the consequent did not result in a terminator, create a
    // branch to the merge block.
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(mergeBB);

    for (IfStmt::iterator I = ite->beginElsif(); I != ite->endElsif(); ++I) {
        Builder.SetInsertPoint(elseBB);
        IfStmt::iterator J = I;
        if (++J != ite->endElsif())
            elseBB = SRF->makeBasicBlock("elsif");
        else if (ite->hasAlternate())
            elseBB = SRF->makeBasicBlock("else");
        else
            elseBB = mergeBB;
        llvm::BasicBlock *bodyBB = SRF->makeBasicBlock("body");
        llvm::Value *pred = emitValue(I->getCondition());
        Builder.CreateCondBr(pred, bodyBB, elseBB);
        Builder.SetInsertPoint(bodyBB);
        emitStmt(I->getConsequent());
        if (!Builder.GetInsertBlock()->getTerminator())
            Builder.CreateBr(mergeBB);
    }

    if (ite->hasAlternate()) {
        Builder.SetInsertPoint(elseBB);
        emitStmt(ite->getAlternate());
        if (!Builder.GetInsertBlock()->getTerminator())
            Builder.CreateBr(mergeBB);
    }

    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitAssignmentStmt(AssignmentStmt *stmt)
{
    Expr *rhs = stmt->getAssignedExpr();

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(stmt->getTarget())) {
        Type *refTy = ref->getType();

        if (isa<ArrayType>(refTy)) {
            // Evaluate the rhs into the storage provided by the lhs.
            llvm::Value *dst =
                SRF->lookup(ref->getDeclaration(), activation::Slot);
            if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(rhs))
                emitCompositeCall(call, dst);
            else
                emitArrayExpr(rhs, dst, false);
        }
        else {
            // The left hand side is a simple variable reference.  Just emit the
            // left and right hand sides and form a store.
            llvm::Value *target = emitVariableReference(ref);
            llvm::Value *source = emitValue(rhs);
            Builder.CreateStore(source, target);
        }
    }
    else {
        // Otherwise, the target must be an IndexedArrayExpr.  Get a reference
        // to the needed component and again, store in the right hand side.
        IndexedArrayExpr *arrIdx = cast<IndexedArrayExpr>(stmt->getTarget());
        llvm::Value *target = emitIndexedArrayRef(arrIdx);
        llvm::Value *source = emitValue(rhs);
        Builder.CreateStore(source, target);
    }
}

void CodeGenRoutine::emitWhileStmt(WhileStmt *stmt)
{
    llvm::BasicBlock *entryBB = SRF->makeBasicBlock("while.top");
    llvm::BasicBlock *bodyBB = SRF->makeBasicBlock("while.entry");
    llvm::BasicBlock *mergeBB = SRF->makeBasicBlock("while.merge");

    // Branch unconditionally from the current insertion point to the entry
    // block and set up our new context.  Emit the loop condition into the entry
    // block.
    Builder.CreateBr(entryBB);
    Builder.SetInsertPoint(entryBB);
    llvm::Value *condition = emitValue(stmt->getCondition());

    // Branch into bodyBB if condition evaluates to true, mergeBB otherwise.
    Builder.CreateCondBr(condition, bodyBB, mergeBB);

    // Generate the body.
    Builder.SetInsertPoint(bodyBB);
    SRF->pushFrame();
    emitStmt(stmt->getBody());
    SRF->popFrame();

    // If the current insertion point does not have a terminator,
    // unconditionally branch to entryBB.
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(entryBB);

    // Finally, set mergeBB the current insertion point.
    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitForStmt(ForStmt *loop)
{
    llvm::Value *iter;          // Iteration variable for the loop.
    llvm::Value *sentinal;      // Iteration bound.

    DSTDefinition *control = loop->getControl();
    if (control->definedUsingAttrib()) {
        std::pair<llvm::Value*, llvm::Value*> bounds;
        RangeAttrib *attrib = control->getAttrib();
        bounds = emitRangeAttrib(attrib);
        iter = bounds.first;
        sentinal = bounds.second;
    }
    else {
        BoundsEmitter emitter(*this);
        BoundsEmitter::LUPair bounds;
        bounds = emitter.getScalarBounds(Builder, control->getType());
        iter = bounds.first;
        sentinal = bounds.second;
    }

    // If the loop is reversed, exchange the iteration variable and bound.
    if (loop->isReversed())
        std::swap(iter, sentinal);

    // We have the initial value for the iteration variable and the bound for
    // the iteration.  Emit an entry, iterate, body, and merge block for the
    // loop.
    llvm::BasicBlock *dominatorBB = Builder.GetInsertBlock();
    llvm::BasicBlock *entryBB = SRF->makeBasicBlock("for.top");
    llvm::BasicBlock *iterBB = SRF->makeBasicBlock("for.iter");
    llvm::BasicBlock *bodyBB = SRF->makeBasicBlock("for.body");
    llvm::BasicBlock *mergeBB = SRF->makeBasicBlock("for.merge");
    llvm::Value *pred;
    llvm::Value *iterSlot;
    llvm::Value *next;
    llvm::PHINode *phi;
    const llvm::Type *iterTy = iter->getType();

    // First, allocate a temporary to hold the iteration variable and
    // initialize.
    iterSlot = SRF->createTemp(iterTy);
    Builder.CreateStore(iter, iterSlot);

    // First, check if the iteration variable is outside the bound, respecting
    // the direction of the iteration.  This condition arrises when we have a
    // null range.  When the range is non-null the iteration variable is valid
    // for at least one loop, hense the branch directly into the body.
    if (loop->isReversed())
        pred = Builder.CreateICmpSLT(iter, sentinal);
    else
        pred = Builder.CreateICmpSGT(iter, sentinal);
    Builder.CreateCondBr(pred, mergeBB, bodyBB);

    // Emit the iteration test.  Since the body has been executed once, a test
    // for equality between the iteration variable and sentinal determines loop
    // termination.  If the iteration continues, we branch to iterBB and
    // increment the iteration variable for the next pass thru the body.  Note
    // too that we must load the iteration value from its slot to pick up its
    // adjusted value.
    Builder.SetInsertPoint(entryBB);
    next = Builder.CreateLoad(iterSlot);
    pred = Builder.CreateICmpEQ(next, sentinal);
    Builder.CreateCondBr(pred, mergeBB, iterBB);

    // Adjust the iteration variable respecting the iteration direction.
    Builder.SetInsertPoint(iterBB);
    if (loop->isReversed())
        next = Builder.CreateSub(next, llvm::ConstantInt::get(iterTy, 1));
    else
        next = Builder.CreateAdd(next, llvm::ConstantInt::get(iterTy, 1));
    Builder.CreateStore(next, iterSlot);
    Builder.CreateBr(bodyBB);

    // Emit the body of the loop and terminate the resulting insertion point
    // with a branch to the entry block.
    Builder.SetInsertPoint(bodyBB);
    SRF->pushFrame();
    phi = Builder.CreatePHI(iterTy, "loop.param");
    phi->addIncoming(iter, dominatorBB);
    phi->addIncoming(next, iterBB);
    SRF->associate(loop->getLoopDecl(), activation::Slot, phi);
    emitStmtSequence(loop->getBody());
    SRF->popFrame();
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(entryBB);

    // Finally, set the insertion point to the mergeBB.
    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitLoopStmt(LoopStmt *stmt)
{
    llvm::BasicBlock *bodyBB = SRF->makeBasicBlock("loop.body");
    llvm::BasicBlock *mergeBB = SRF->makeBasicBlock("loop.merge");

    // Branch unconditionally from the current insertion point to the loop body.
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(bodyBB);

    // Generate the body.
    SRF->pushFrame();
    emitStmt(stmt->getBody());
    SRF->popFrame();

    // If the current insertion point does not have a terminator,
    // unconditionally branch back to the start of the body.
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(bodyBB);

    // Finally, set mergeBB as the current insertion point.
    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitRaiseStmt(RaiseStmt *stmt)
{
    ExceptionDecl *exception = stmt->getExceptionDecl();

    if (stmt->hasMessage()) {
        BoundsEmitter emitter(*this);
        std::pair<llvm::Value*, llvm::Value*> ABPair;
        llvm::Value *message;
        llvm::Value *length;
        ABPair = emitArrayExpr(stmt->getMessage(), 0, false);
        message = ABPair.first;
        length = emitter.computeBoundLength(Builder, ABPair.second, 0);
        CG.getRuntime().raise(Builder, exception, message, length);
    }
    else
        CG.getRuntime().raise(Builder, exception, 0, 0);
}

void CodeGenRoutine::emitPragmaStmt(PragmaStmt *stmt)
{
    // Currently, only pragma Assert is supported.
    Pragma *pragma = stmt->getPragma();

    switch (pragma->getKind()) {

    default:
        assert(false && "Cannot codegen pragma yet!");
        break;

    case pragma::Assert:
        emitPragmaAssert(cast<PragmaAssert>(pragma));
        break;
    };
}
