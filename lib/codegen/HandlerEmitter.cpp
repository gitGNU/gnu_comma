//===-- codegen/HandlerEmitter.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenRoutine.h"
#include "CommaRT.h"
#include "HandlerEmitter.h"
#include "comma/ast/ExceptionRef.h"
#include "comma/ast/Stmt.h"

using namespace comma;

HandlerEmitter::HandlerEmitter(CodeGenRoutine &CGR)
    : CGR(CGR), CG(CGR.getCodeGen()), RT(CG.getRuntime()) { }

Frame *HandlerEmitter::frame() { return CGR.getFrame(); }

llvm::Value *HandlerEmitter::emitSelector(llvm::Value *exception,
                                          StmtSequence *seq)
{
    typedef StmtSequence::handler_iter handler_iter;
    typedef HandlerStmt::choice_iterator choice_iter;
    llvm::SmallVector<llvm::Value*, 8> args;

    // Mandatory args to llvm.eh.selector.
    args.push_back(exception);
    args.push_back(RT.getEHPersonality());

    // Push an exinfo object for each named exception choice.
    for (handler_iter H = seq->handler_begin(); H != seq->handler_end(); ++H) {
        HandlerStmt *handler = *H;
        choice_iter C = handler->choice_begin();
        choice_iter E = handler->choice_end();
        for ( ; C != E; ++C) {
            ExceptionDecl *target = (*C)->getException();
            args.push_back(RT.registerException(target));
        }
    }

    // Always emit a catch-all.  We simply resume the exception occurrence if
    // there isn't an actual catch-all handler present.
    args.push_back(CG.getNullPointer(CG.getInt8PtrTy()));

    return frame()->getIRBuilder().CreateCall(
        CG.getEHSelectorIntrinsic(), args.begin(), args.end());
}

void HandlerEmitter::emitHandlers(StmtSequence *seq, llvm::BasicBlock *mergeBB)
{
    if (mergeBB == 0)
        mergeBB = frame()->makeBasicBlock("landingpad.merge");

    llvm::IRBuilder<> &Builder = frame()->getIRBuilder();
    llvm::BasicBlock *landingPad = frame()->getLandingPad();

    // Remove the landing pad the current set of handlers target.  This is
    // required to ensure that any exceptions thrown in the handlers themselves
    // propagate.
    frame()->removeLandingPad();

    // Obtain the thrown exception object and the selection index.
    Builder.SetInsertPoint(landingPad);
    llvm::Value *exception = Builder.CreateCall(CG.getEHExceptionIntrinsic());
    llvm::Value *infoIdx = emitSelector(exception, seq);
    llvm::Value *eh_typeid = CG.getEHTypeidIntrinsic();
    llvm::BasicBlock *lpadBB = frame()->makeBasicBlock("lpad");
    Builder.CreateBr(lpadBB);
    Builder.SetInsertPoint(lpadBB);

    for (StmtSequence::handler_iter H = seq->handler_begin();
         H != seq->handler_end(); ++H) {
        HandlerStmt *handler = *H;
        llvm::BasicBlock *bodyBB = frame()->makeBasicBlock("lpad.body");

        // Catch-all's can simply branch directly to the handler body.
        if (handler->isCatchAll())
            Builder.CreateBr(bodyBB);
        else {
            for (HandlerStmt::choice_iterator C = handler->choice_begin();
                 C != handler->choice_end(); ++C) {
                ExceptionDecl *target = (*C)->getException();
                llvm::Value *exinfo = RT.registerException(target);
                llvm::Value *targetIdx = Builder.CreateCall(eh_typeid, exinfo);
                llvm::Value *pred = Builder.CreateICmpEQ(infoIdx, targetIdx);
                lpadBB = frame()->makeBasicBlock("lpad");
                Builder.CreateCondBr(pred, bodyBB, lpadBB);
                Builder.SetInsertPoint(lpadBB);
            }
        }

        // Emit the body for this handler, then switch back to the next landing
        // pad.
        Builder.SetInsertPoint(bodyBB);
        for (StmtSequence::stmt_iter I = handler->stmt_begin();
             I != handler->stmt_end(); ++I)
            CGR.emitStmt(*I);
        if (!Builder.GetInsertBlock()->getTerminator())
            Builder.CreateBr(mergeBB);
        Builder.SetInsertPoint(lpadBB);
    }

    // We always match a thrown exception.  If there isn't a catch-all handler
    // explicitly associated with this sequence then the current insertion point
    // is set to our implicit handler.  Propagate the exception.
    if (!seq->hasCatchAll())
        RT.reraise(frame(), exception);

    Builder.SetInsertPoint(mergeBB);
}



