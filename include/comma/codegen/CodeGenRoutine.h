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
    CodeGenRoutine(CodeGenCapsule &CGC);

    void declareSubroutine(SubroutineDecl *srDecl);
    void emitSubroutine(SubroutineDecl *srDecl);

private:
    // Emits the prologue for the current subroutine given a basic block
    // representing the body of the function.
    void emitPrologue(llvm::BasicBlock *body);

    // Emits the epilogue for the current subroutine.
    void emitEpilogue();

    /// Given the current SubroutineDecl and llvm::Function, initialize
    /// CodeGenRoutine::percent with the llvm value corresponding to the first
    /// (implicit) argument.  Also, name the llvm arguments after the source
    /// formals, and populate the lookup tables such that a search for a
    /// parameter decl yields the corresponding llvm value.
    void injectSubroutineArgs();

    /// Generates code for the current subroutines body.
    void emitSubroutineBody();

    void emitObjectDecl(ObjectDecl *objDecl);

    void emitStmt(Stmt *stmt);
    void emitIfStmt(IfStmt *ite);
    void emitReturnStmt(ReturnStmt *ret);
    void emitStmtSequence(StmtSequence *seq);
    void emitProcedureCallStmt(ProcedureCallStmt *stmt);
    void emitAssignmentStmt(AssignmentStmt *stmt);

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
    llvm::Value *emitPrjExpr(PrjExpr *expr);
    llvm::Value *emitInjExpr(InjExpr *expr);

    llvm::Value *emitFunctionCall(FunctionCallExpr *expr);

    llvm::Value *emitPrimitiveCall(FunctionCallExpr *expr,
                                   std::vector<llvm::Value *> &args);

    llvm::Value *emitLocalCall(SubroutineDecl *srDecl,
                               std::vector<llvm::Value *> &args);

    llvm::Value *emitDirectCall(SubroutineDecl *srDecl,
                                std::vector<llvm::Value *> &args);

    llvm::Value *emitCallArgument(SubroutineDecl *srDecl, Expr *arg,
                                  unsigned argPosition);

    llvm::Value *lookupDecl(Decl *decl);

    /// Returns true if the given call is "direct", meaning that the domain of
    /// computation is staticly known.
    static bool isDirectCall(const FunctionCallExpr *expr);

    /// Returns true if the given call is "direct", meaning that the domain of
    /// computation is staticly known.
    static bool isDirectCall(const ProcedureCallStmt *stmt);

    /// Returns true if the given call is "local", meaning that the call is to a
    /// function defined in the current capsule.
    static bool isLocalCall(const FunctionCallExpr *expr);

    /// Returns true if the given call is "local", meaning that the call is to a
    /// function defined in the current capsule.
    static bool isLocalCall(const ProcedureCallStmt *stmt);

    llvm::Function *getOrCreateSubroutineDeclaration(SubroutineDecl *srDecl);

    llvm::Value *emitScalarLoad(llvm::Value *ptr);

    llvm::Value *getStackSlot(Decl *decl);

    llvm::Value *emitVariableReference(Expr *expr);

    llvm::Value *emitValue(Expr *expr);

    llvm::Value *createStackSlot(Decl *decl);

    llvm::Value *getOrCreateStackSlot(Decl *decl);
};

} // end comma namespace

#endif
