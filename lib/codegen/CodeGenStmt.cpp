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

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void CodeGenRoutine::emitStmt(Stmt *stmt)
{
    // FIXME: This is just temporary code which handles simple return
    // statements.
    if (ReturnStmt *ret = dyn_cast<ReturnStmt>(stmt)) {
        if (ret->hasReturnExpr()) {
            // Store the result into the return slot.
            assert(returnValue && "Non-empty return from function!");
            Expr *expr = ret->getReturnExpr();
            llvm::Value *res = emitExpr(expr);
            Builder.CreateStore(res, returnValue);
            Builder.CreateBr(returnBB);
        }
        else {
            // We should be emitting for a procedure.  Simply branch to the
            // return block.
            assert(returnValue == 0 && "Empty return from function!");
            Builder.CreateBr(returnBB);
        }
    }
}

llvm::BasicBlock *CodeGenRoutine::emitBlock(BlockStmt        *block,
                                            llvm::BasicBlock *predecessor)
{
    assert(block && "NULL block statement!");
    llvm::BasicBlock *BB;

    // Create a basic block and make it the current insertion point.
    {
        std::string label;

        if (predecessor == 0)
            predecessor = Builder.GetInsertBlock();

        if (block->hasLabel())
            label = block->getLabel()->getString();

        BB = llvm::BasicBlock::Create(label, SRFn);
        BB->moveAfter(predecessor);
        Builder.SetInsertPoint(BB);
    }

    for (BlockStmt::StmtIter iter = block->beginStatements();
         iter != block->endStatements(); ++iter)
        emitStmt(*iter);

    return BB;
}
