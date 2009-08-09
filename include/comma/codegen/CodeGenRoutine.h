//===-- codegen/CodeGenRoutine.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENROUTINE_HDR_GUARD
#define COMMA_CODEGEN_CODEGENROUTINE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/codegen/CodeGen.h"

#include "llvm/DerivedTypes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/IRBuilder.h"

namespace llvm {

class BasicBlock;
class Function;

} // end namespace llvm;

namespace comma {

// This class provides for code generation of subroutines.
class CodeGenRoutine {

    CodeGenCapsule &CGC;
    CodeGen        &CG;
    CodeGenTypes   &CGTypes;
    const CommaRT  &CRT;

    // Builder object used to construct LLVM IR.
    llvm::IRBuilder<> Builder;

    // The subroutine declaration we are emitting code for.
    SubroutineDecl *SRDecl;

    // The llvm function we are emitting code into.
    llvm::Function *SRFn;

    // The first (implicit) argument to this function (%).
    llvm::Value *percent;

    // The entry block for the subroutine.
    llvm::BasicBlock *entryBB;

    // The return block for the subroutine.
    llvm::BasicBlock *returnBB;

    // The return value for the subroutine, represented as an alloca'd stack
    // slot.  If we generating a procedure, this member is null.
    llvm::Value *returnValue;

    // Map from Comma decl's to corresponding LLVM values.
    typedef llvm::DenseMap<Decl *, llvm::Value *> DeclMap;
    DeclMap declTable;

public:
    CodeGenRoutine(CodeGenCapsule &CGC, SubroutineDecl *SR);

private:
    // Emits the associated subroutine decl into llvm IR.
    void emit();

    // Emits the prologue for the current subroutine given a basic block
    // representing the body of the function.
    void emitPrologue(llvm::BasicBlock *body);

    // Emits the epilogue for the current subroutine.
    void emitEpilogue();


    void emitStmt(Stmt *stmt);
    void emitIfStmt(IfStmt *ite);
    void emitReturnStmt(ReturnStmt *ret);
    void emitStmtSequence(StmtSequence *seq);

    /// Generates code for the given BlockStmt.
    ///
    /// If \p predecessor is not null, then this method generates a BasicBlock
    /// assuming that the caller will construct the appropriate instructions
    /// necessary to ensure the block generated is reachable.  Otherwise, the
    /// current insertion block advertised thru the IRBuiler is taken and an
    /// unconditional branch to the generated block is appended.
    llvm::BasicBlock *emitBlockStmt(BlockStmt *block,
                                    llvm::BasicBlock *predecessor = 0);

    llvm::Value *emitExpr(Expr *expr);
    llvm::Value *emitDeclRefExpr(DeclRefExpr *expr);
    llvm::Value *emitFunctionCall(FunctionCallExpr *expr);
    llvm::Value *emitPrimitiveCall(FunctionCallExpr *expr,
                                   std::vector<llvm::Value *> &args);
    llvm::Value *emitPrjExpr(PrjExpr *expr);
    llvm::Value *emitInjExpr(InjExpr *expr);

    llvm::Value *lookupDecl(Decl *decl);

    // Returns true if the given call is "direct", meaning that the domain of
    // computation is known.
    static bool isDirectCall(const FunctionCallExpr *expr);

    static bool isLocalCall(const FunctionCallExpr *expr);
};

} // end comma namespace

#endif
