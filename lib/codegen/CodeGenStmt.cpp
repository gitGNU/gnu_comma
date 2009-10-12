//===-- codege/CodeGenStmt.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"

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
        emitBlockStmt(cast<BlockStmt>(stmt));
        break;

    case Ast::AST_IfStmt:
        emitIfStmt(cast<IfStmt>(stmt));
        break;

    case Ast::AST_WhileStmt:
        emitWhileStmt(cast<WhileStmt>(stmt));
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
    if (ret->hasReturnExpr()) {
        // Store the result into the return slot.
        assert(returnValue && "Non-empty return from function!");
        Expr *expr = ret->getReturnExpr();
        llvm::Value *res = emitValue(ret->getReturnExpr());

        // If the return value is composite, copy the result into the return
        // value, otherwise a simple store suffices.
        if (ArraySubType *arrTy = dyn_cast<ArraySubType>(expr->getType()))
            emitArrayCopy(res, returnValue, arrTy);
        else
            Builder.CreateStore(res, returnValue);
        Builder.CreateBr(returnBB);
    }
    else {
        // We should be emitting for a procedure.  Simply branch to the return
        // block.
        assert(returnValue == 0 && "Empty return from function!");
        Builder.CreateBr(returnBB);
    }
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

    llvm::BasicBlock *BB = CG.makeBasicBlock(label, SRFn);

    if (predecessor == 0) {
        // If we are not supplied a predecessor, terminate the current
        // insertion block with a direct branch to the new one.
        predecessor = Builder.GetInsertBlock();

        assert(!predecessor->getTerminator() &&
               "Current insertion block already terminated!");
        Builder.CreateBr(BB);
    }

    BB->moveAfter(predecessor);
    Builder.SetInsertPoint(BB);

    // Generate any object declarations provided by this block, followed by the
    // blocks sequence of statements.
    typedef DeclRegion::DeclIter iterator;
    for (iterator I = block->beginDecls(); I != block->endDecls(); ++I)
        if (ObjectDecl *objDecl = dyn_cast<ObjectDecl>(*I))
            emitObjectDecl(objDecl);
    emitStmtSequence(block);
    return BB;
}

void CodeGenRoutine::emitIfStmt(IfStmt *ite)
{
    llvm::Value *condition = emitExpr(ite->getCondition());
    llvm::BasicBlock *thenBB = CG.makeBasicBlock("then", SRFn);
    llvm::BasicBlock *mergeBB = CG.makeBasicBlock("merge", SRFn);
    llvm::BasicBlock *elseBB;

    if (ite->hasElsif())
        elseBB = CG.makeBasicBlock("elsif", SRFn);
    else if (ite->hasAlternate())
        elseBB = CG.makeBasicBlock("else", SRFn);
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
            elseBB = CG.makeBasicBlock("elsif", SRFn);
        else if (ite->hasAlternate())
            elseBB = CG.makeBasicBlock("else", SRFn);
        else
            elseBB = mergeBB;
        llvm::BasicBlock *bodyBB = CG.makeBasicBlock("body", SRFn);
        llvm::Value *pred = emitExpr(I->getCondition());
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

void CodeGenRoutine::emitProcedureCallStmt(ProcedureCallStmt *stmt)
{
    ProcedureDecl *pdecl = stmt->getConnective();
    std::vector<llvm::Value *> args;

    typedef ProcedureCallStmt::arg_iterator iterator;
    iterator I = stmt->begin_arguments();
    iterator E = stmt->end_arguments();
    for (unsigned i = 0; I != E; ++I, ++i)
        emitCallArgument(pdecl, *I, i, args);

    if (isLocalCall(stmt))
        emitLocalCall(pdecl, args);
    else if (isDirectCall(stmt))
        emitDirectCall(pdecl, args);
    else
        emitAbstractCall(pdecl, args);
}

void CodeGenRoutine::emitAssignmentStmt(AssignmentStmt *stmt)
{
    llvm::Value *target;
    llvm::Value *source;
    Expr *rhs = stmt->getAssignedExpr();

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(stmt->getTarget())) {
        // The left hand side is a simple variable reference.  Just emit the
        // left and right hand sides and form a store.
        target = emitVariableReference(ref);
        source = emitValue(rhs);
    }
    else {
        // Otherwise, the target must be an IndexedArrayExpr.  Get a reference
        // to the needed component and again, store in the right hand side.
        IndexedArrayExpr *arrIdx = cast<IndexedArrayExpr>(stmt->getTarget());
        target = emitIndexedArrayRef(arrIdx);
        source = emitValue(rhs);
    }

    Builder.CreateStore(source, target);
}

void CodeGenRoutine::emitWhileStmt(WhileStmt *stmt)
{
    llvm::BasicBlock *entryBB = CG.makeBasicBlock("while.top", SRFn);
    llvm::BasicBlock *bodyBB = CG.makeBasicBlock("while.entry", SRFn);
    llvm::BasicBlock *mergeBB = CG.makeBasicBlock("while.merge", SRFn);

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
