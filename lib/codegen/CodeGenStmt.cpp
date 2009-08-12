//===-- codege/CodeGenStmt.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
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

    case Ast::AST_ReturnStmt:
        emitReturnStmt(cast<ReturnStmt>(stmt));
        break;
    }
}

void CodeGenRoutine::emitReturnStmt(ReturnStmt *ret)
{
    if (ret->hasReturnExpr()) {
        // Store the result into the return slot.
        assert(returnValue && "Non-empty return from function!");
        llvm::Value *res = emitValue(ret->getReturnExpr());
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

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(label, SRFn);

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
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create("then", SRFn);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create("merge", SRFn);
    llvm::BasicBlock *elseBB;

    if (ite->hasElsif())
        elseBB = llvm::BasicBlock::Create("elsif", SRFn);
    else if (ite->hasAlternate())
        elseBB = llvm::BasicBlock::Create("else", SRFn);
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
            elseBB = llvm::BasicBlock::Create("elsif", SRFn);
        else if (ite->hasAlternate())
            elseBB = llvm::BasicBlock::Create("else", SRFn);
        else
            elseBB = mergeBB;
        llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create("body", SRFn);
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

    for (unsigned i = 0; i < stmt->getNumArgs(); ++i) {
        Expr *arg = stmt->getArg(i);
        args.push_back(emitCallArgument(pdecl, arg, i));
    }

    if (isLocalCall(stmt))
        emitLocalCall(pdecl, args);
    else if (isDirectCall(stmt))
        emitDirectCall(pdecl, args);
    else
        CRT.genAbstractCall(Builder, percent, pdecl, args);
}

void CodeGenRoutine::emitAssignmentStmt(AssignmentStmt *stmt)
{
    llvm::Value *ref = emitVariableReference(stmt->getTarget());
    llvm::Value *rhs = emitValue(stmt->getAssignedExpr());

    Builder.CreateStore(rhs, ref);
}
