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
    Type *targetTy = fTy->getReturnType();
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
                    emitArrayExpr(expr, 0, true);
                // First push the data, then the bounds.  This allows the bounds
                // to be accessed first by the callee and the size of the data
                // computed before the data itself is retrieved.
                llvm::Value *data = arrayPair.first;
                llvm::Value *bounds = arrayPair.second;
                llvm::Value *dataSize =
                    emitter.computeTotalBoundLength(Builder, bounds);
                llvm::Value *boundSize =
                    llvm::ConstantExpr::getSizeOf(bounds->getType());
                llvm::Value *boundSlot = SRF->createTemp(bounds->getType());

                // getSizeOf returns an i64 but vstack_push requires an i32.
                boundSize = Builder.CreateTrunc(boundSize, CG.getInt32Ty());

                // Store the expressions bounds into a the allocated slot.
                Builder.CreateStore(bounds, boundSlot);

                CRT.vstack_push(Builder, data, dataSize);
                CRT.vstack_push(Builder, boundSlot, boundSize);
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
    for (StmtSequence::StmtIter iter = seq->beginStatements();
         iter != seq->endStatements(); ++iter)
        emitStmt(*iter);
}

llvm::BasicBlock *CodeGenRoutine::emitBlockStmt(BlockStmt *block,
                                                llvm::BasicBlock *predecessor)
{
    assert(block && "NULL block statement!");
    std::string label;

    if (block->hasLabel())
        label = block->getLabel()->getString();

    llvm::BasicBlock *BB = makeBasicBlock(label);

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
    llvm::BasicBlock *thenBB = makeBasicBlock("then");
    llvm::BasicBlock *mergeBB = makeBasicBlock("merge");
    llvm::BasicBlock *elseBB;

    if (ite->hasElsif())
        elseBB = makeBasicBlock("elsif");
    else if (ite->hasAlternate())
        elseBB = makeBasicBlock("else");
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
            elseBB = makeBasicBlock("elsif");
        else if (ite->hasAlternate())
            elseBB = makeBasicBlock("else");
        else
            elseBB = mergeBB;
        llvm::BasicBlock *bodyBB = makeBasicBlock("body");
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
    llvm::BasicBlock *entryBB = makeBasicBlock("while.top");
    llvm::BasicBlock *bodyBB = makeBasicBlock("while.entry");
    llvm::BasicBlock *mergeBB = makeBasicBlock("while.merge");

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
    emitStmt(stmt->getBody());

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

    if (RangeAttrib *control = loop->getAttribControl()) {
        std::pair<llvm::Value*, llvm::Value*> bounds;
        bounds = emitRangeAttrib(control);
        iter = bounds.first;
        sentinal = bounds.second;
    }
    else {
        Range *range = loop->getRangeControl();
        iter = emitValue(range->getLowerBound());
        sentinal = emitValue(range->getUpperBound());
    }

    // If the loop is reversed, exchange the iteration variable and bound.
    if (loop->isReversed())
        std::swap(iter, sentinal);

    // We have the initial value for the iteration variable and the bound for
    // the iteration.  Emit an entry, iterate, body, and merge block for the
    // loop.
    llvm::BasicBlock *dominatorBB = Builder.GetInsertBlock();
    llvm::BasicBlock *entryBB = makeBasicBlock("for.top");
    llvm::BasicBlock *iterBB = makeBasicBlock("for.iter");
    llvm::BasicBlock *bodyBB = makeBasicBlock("for.body");
    llvm::BasicBlock *mergeBB = makeBasicBlock("for.merge");
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
    //
    // FIXME: Once nested frames are available, the iteration binding should be
    // scoped.
    Builder.SetInsertPoint(bodyBB);
    phi = Builder.CreatePHI(iterTy, "loop.param");
    phi->addIncoming(iter, dominatorBB);
    phi->addIncoming(next, iterBB);
    SRF->associate(loop->getLoopDecl(), activation::Slot, phi);
    emitStmtSequence(loop->getBody());
    Builder.CreateBr(entryBB);

    // Finally, set the insertion point to the mergeBB.
    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitLoopStmt(LoopStmt *stmt)
{
    llvm::BasicBlock *bodyBB = makeBasicBlock("loop.body");
    llvm::BasicBlock *mergeBB = makeBasicBlock("loop.merge");

    // Branch unconditionally from the current insertion point to the loop body.
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(bodyBB);

    // Generate the body.
    emitStmt(stmt->getBody());

    // If the current insertion point does not have a terminator,
    // unconditionally branch back to the start of the body.
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(bodyBB);

    // Finally, set mergeBB as the current insertion point.
    Builder.SetInsertPoint(mergeBB);
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
