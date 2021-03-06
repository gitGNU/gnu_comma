//===-- codege/CodeGenStmt.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "HandlerEmitter.h"
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

    case Ast::AST_ExitStmt:
        emitExitStmt(cast<ExitStmt>(stmt));
        break;

    case Ast::AST_PragmaStmt:
        emitPragmaStmt(cast<PragmaStmt>(stmt));
        break;

    case Ast::AST_NullStmt:
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
        // Constrained arrays are emitted directly into the sret parameter.
        // Unconstrained arrays are propagated thru the vstack.
        if (arrTy->isConstrained())
            emitArrayExpr(expr, returnValue, false);
        else {
            assert(returnValue == 0);
            emitVStackReturn(expr, arrTy);
        }
    }
    else if (targetTy->isRecordType()) {
        // Emit records directly into the sret return parameter.
        emitRecordExpr(expr, returnValue, false);
    }
    else if (targetTy->isFatAccessType()) {
        // Fat access types are represented as pointers to their underlying
        // structure.
        llvm::Value *res = emitValue(ret->getReturnExpr()).first();
        Builder.CreateStore(Builder.CreateLoad(res), returnValue);
    }
    else {
        // Non-composite return values can be simply evaluated and stored into
        // this subroutines return slot.
        llvm::Value *res = emitValue(ret->getReturnExpr()).first();
        Builder.CreateStore(res, returnValue);
    }

    // Branch to the return block.
    SRF->emitReturn();
}

void CodeGenRoutine::emitVStackReturn(Expr *expr, ArrayType *arrTy)
{
    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(expr)) {
        emitSimpleCall(call);
        return;
    }

    BoundsEmitter emitter(*this);
    CValue arrValue = emitArrayExpr(expr, 0, false);
    const llvm::Type *componentTy = CGT.lowerType(arrTy->getComponentType());
    llvm::Value *data = arrValue.first();
    llvm::Value *bounds = arrValue.second();
    llvm::Value *length = emitter.computeTotalBoundLength(Builder, bounds);
    const llvm::Type *boundsTy = bounds->getType();

    // Compute the size of the data.  Note that getSizeOf returns an i64 but we
    // need an i32 here.
    llvm::Value *dataSize;
    dataSize = llvm::ConstantExpr::getSizeOf(componentTy);
    dataSize = Builder.CreateTrunc(dataSize, CG.getInt32Ty());
    dataSize = Builder.CreateMul(dataSize, length);

    // Compute the size of the bounds structure.  This is dependent on if the
    // bounds were given as a pointer or an aggregate.
    llvm::Value *boundsSize;
    if (boundsTy->isAggregateType())
        boundsSize = llvm::ConstantExpr::getSizeOf(boundsTy);
    else {
        const llvm::PointerType *ptr;
        ptr = llvm::cast<llvm::PointerType>(boundsTy);
        boundsSize = llvm::ConstantExpr::getSizeOf(ptr->getElementType());
    }
    boundsSize = Builder.CreateTrunc(boundsSize, CG.getInt32Ty());

    // Generate a temporary if needed so that we can obtain a pointer to the
    // bounds.
    if (boundsTy->isAggregateType()) {
        llvm::Value *slot = SRF->createTemp(boundsTy);
        Builder.CreateStore(bounds, slot);
        bounds = slot;
    }

    CRT.vstack_push(Builder, data, dataSize);
    CRT.vstack_push(Builder, bounds, boundsSize);
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

    llvm::BasicBlock *BB;
    if (block->hasLabel()) {
        const char *label = block->getLabel()->getString();
        BB = SRF->pushFrame(Subframe::Block, label);
    }
    else
        BB = SRF->pushFrame(Subframe::Block);

    if (block->isHandled())
        SRF->addLandingPad();

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

        case Ast::AST_RenamedObjectDecl:
            emitRenamedObjectDecl(cast<RenamedObjectDecl>(decl));
            break;
        }
    }
    emitStmtSequence(block);

    // Terminate the current insertion point to the unified merge block if
    // needed.
    llvm::BasicBlock *mergeBB = SRF->subframe()->getMergeBB();
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(mergeBB);

    // Emit exception handlers if needed.
    if (block->isHandled()) {
        HandlerEmitter emitter(*this);
        emitter.emitHandlers(block, mergeBB);
    }

    // Set the insertion point to the merge block.
    SRF->popFrame();
    Builder.SetInsertPoint(mergeBB);
    return BB;
}

void CodeGenRoutine::emitIfStmt(IfStmt *ite)
{
    CValue condition = emitValue(ite->getCondition());
    llvm::BasicBlock *thenBB = SRF->makeBasicBlock("then");
    llvm::BasicBlock *mergeBB = SRF->makeBasicBlock("merge");
    llvm::BasicBlock *elseBB;

    if (ite->hasElsif())
        elseBB = SRF->makeBasicBlock("elsif");
    else if (ite->hasAlternate())
        elseBB = SRF->makeBasicBlock("else");
    else
        elseBB = mergeBB;

    Builder.CreateCondBr(condition.first(), thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);

    // Emit the true branch in its own frame. If generation of the consequent
    // did not result in a terminator, create a branch to the merge block.
    SRF->pushFrame(Subframe::Block, thenBB, mergeBB);
    emitStmt(ite->getConsequent());
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(mergeBB);
    SRF->popFrame();

    // Emit each elsif block in its own frame.
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
        llvm::Value *pred = emitValue(I->getCondition()).first();
        Builder.CreateCondBr(pred, bodyBB, elseBB);
        Builder.SetInsertPoint(bodyBB);

        SRF->pushFrame(Subframe::Block, bodyBB, mergeBB);
        emitStmt(I->getConsequent());
        if (!Builder.GetInsertBlock()->getTerminator())
            Builder.CreateBr(mergeBB);
        SRF->popFrame();
    }

    if (ite->hasAlternate()) {
        Builder.SetInsertPoint(elseBB);

        SRF->pushFrame(Subframe::Block, elseBB, mergeBB);
        emitStmt(ite->getAlternate());
        if (!Builder.GetInsertBlock()->getTerminator())
            Builder.CreateBr(mergeBB);
        SRF->popFrame();
    }

    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitWhileStmt(WhileStmt *stmt)
{
    llvm::BasicBlock *entryBB = SRF->makeBasicBlock("while.top");
    llvm::BasicBlock *bodyBB = SRF->makeBasicBlock("while.entry");
    llvm::BasicBlock *mergeBB = SRF->makeBasicBlock("while.merge");

    // If this loop is tagged name the body block after it (and the associated
    // subframe).
    if (stmt->isTagged())
        bodyBB->setName(stmt->getTag()->getString());

    // Branch unconditionally from the current insertion point to the entry
    // block and set up our new context.  Emit the loop condition into the entry
    // block.
    Builder.CreateBr(entryBB);
    Builder.SetInsertPoint(entryBB);
    CValue condition = emitValue(stmt->getCondition());

    // Branch into bodyBB if condition evaluates to true, mergeBB otherwise.
    Builder.CreateCondBr(condition.first(), bodyBB, mergeBB);

    // Generate the body.
    Builder.SetInsertPoint(bodyBB);
    SRF->pushFrame(Subframe::Loop, bodyBB, mergeBB);
    emitStmt(stmt->getBody());
    SRF->popFrame();

    // If the current insertion point does not have a terminator,
    // unconditionally branch to entryBB.
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(entryBB);

    // Finally, set mergeBB as the current insertion point.
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

    // If this loop is tagged name the body block after it (and the associated
    // subframe).
    if (loop->isTagged())
        bodyBB->setName(loop->getTag()->getString());

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
    SRF->pushFrame(Subframe::Loop, bodyBB, mergeBB);
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

    // If this loop is tagged name the body block after it (and the associated
    // subframe).
    if (stmt->isTagged())
        bodyBB->setName(stmt->getTag()->getString());

    // Branch unconditionally from the current insertion point to the loop body.
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(bodyBB);

    // Generate the body.
    SRF->pushFrame(Subframe::Loop, bodyBB, mergeBB);
    emitStmt(stmt->getBody());
    SRF->popFrame();

    // If the current insertion point does not have a terminator,
    // unconditionally branch back to the start of the body.
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(bodyBB);

    // Finally, set mergeBB as the current insertion point.
    Builder.SetInsertPoint(mergeBB);
}

void CodeGenRoutine::emitExitStmt(ExitStmt *stmt)
{
    Subframe *loopFrame = 0;

    // If the exit statement is tagged, lookup the corresponding frame by name.
    // Otherwise, associate with the innermost loop frame.
    if (stmt->hasTag()) {
        const char *tag = stmt->getTag()->getString();
        loopFrame = SRF->findFirstNamedSubframe(tag);
    }
    else
        loopFrame = SRF->findFirstSubframe(Subframe::Loop);

    assert(loopFrame && "Invalid context for exit stmt!");

    // Get the basic block the loop dumps into.
    llvm::BasicBlock *mergeBB = loopFrame->getMergeBB();

    // Evaluate the condition if present.
    if (stmt->hasCondition()) {
        llvm::BasicBlock *continuationBB = SRF->makeBasicBlock("exit.continue");

        // If the statement predicate is true, branch to the merge block of the
        // loop.  Otherwise, continue with the current execution path.
        llvm::Value *pred = emitValue(stmt->getCondition()).first();
        Builder.CreateCondBr(pred, mergeBB, continuationBB);
        Builder.SetInsertPoint(continuationBB);
    }
    else
        Builder.CreateBr(mergeBB);
}

void CodeGenRoutine::emitRaiseStmt(RaiseStmt *stmt)
{
    CommaRT &CRT = CG.getRuntime();
    ExceptionDecl *exception = stmt->getExceptionDecl();
    llvm::Value *fileName = CG.getModuleName();
    llvm::Value *lineNum = CG.getSourceLine(stmt->getLocation());

    if (stmt->hasMessage()) {
        BoundsEmitter emitter(*this);
        CValue arrValue = emitArrayExpr(stmt->getMessage(), 0, false);
        llvm::Value *message;
        llvm::Value *length;

        message = arrValue.first();
        length = emitter.computeBoundLength(Builder, arrValue.second(), 0);
        CRT.raise(SRF, exception, fileName, lineNum, message, length);
    }
    else
        CRT.raise(SRF, exception, fileName, lineNum, 0, 0);
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
