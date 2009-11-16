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

    CodeGen        &CG;
    CodeGenCapsule &CGC;
    CodeGenTypes   &CGT;
    const CommaRT  &CRT;

    // Builder object used to construct LLVM IR.
    llvm::IRBuilder<> Builder;

    // The info node for the subroutine we are emitting code for.
    SRInfo *srInfo;

    // The declaration node which is the completion of the current subroutine.
    SubroutineDecl *srCompletion;

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

    // Map from array objects to bound structures.
    DeclMap boundTable;

public:
    CodeGenRoutine(CodeGenCapsule &CGC);

    /// Returns the associated code generator context.
    CodeGen &getCodeGen() { return CG; }

    /// Returns the associated capsule generator context.
    CodeGenCapsule &getCGC() { return CGC; }

    /// \brief Returns the SRInfo object corresponding to the subroutine being
    /// generated.
    SRInfo *getSRInfo() { return srInfo; }

    llvm::Value *getImplicitContext() const { return percent; }

    void emitSubroutine(SubroutineDecl *srDecl);

    llvm::Value *emitValue(Expr *expr);
    llvm::Value *emitVariableReference(Expr *expr);
    std::pair<llvm::Value*, llvm::Value*>
    emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp);

    llvm::Value *emitSimpleCall(FunctionCallExpr *expr);
    void emitCompositeCall(FunctionCallExpr *expr, llvm::Value *dst);

    /// \brief Given an array type with statically constrained indices,
    /// synthesizes a constant LLVM structure representing the bounds of the
    /// array.
    llvm::Constant *synthStaticArrayBounds(ArrayType *arrTy);

    llvm::Value *createTemp(const llvm::Type *type);

    /// Returns true if the given call is "direct", meaning that the domain of
    /// computation is staticly known.
    static bool isDirectCall(const SubroutineCall *expr);

    /// Returns true if the given call is "local", meaning that the call is to a
    /// subroutine defined in the current capsule.
    static bool isLocalCall(const SubroutineCall *expr);

    /// Returns true if the given call is forgien.
    static bool isForeignCall(const SubroutineCall *call);

private:
    // Returns the llvm function we are generating code for.
    llvm::Function *getLLVMFunction() const;

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
    void emitWhileStmt(WhileStmt *stmt);
    void emitPragmaStmt(PragmaStmt *stmt);

    /// Generates code for the given BlockStmt.
    ///
    /// If \p predecessor is not null, then this method generates a BasicBlock
    /// assuming that the caller will construct the appropriate instructions
    /// necessary to ensure the block generated is reachable.  Otherwise, the
    /// current insertion block advertised thru the IRBuiler is taken and an
    /// unconditional branch to the generated block is appended.
    llvm::BasicBlock *emitBlockStmt(BlockStmt *block,
                                    llvm::BasicBlock *predecessor = 0);

    llvm::Value *emitDeclRefExpr(DeclRefExpr *expr);
    llvm::Value *emitPrjExpr(PrjExpr *expr);
    llvm::Value *emitInjExpr(InjExpr *expr);
    llvm::Value *emitIntegerLiteral(IntegerLiteral *expr);
    llvm::Value *emitIndexedArrayValue(IndexedArrayExpr *expr);
    llvm::Value *emitConversionValue(ConversionExpr *expr);
    llvm::Value *emitAttribExpr(AttribExpr *expr);


    llvm::Value *emitIndexedArrayRef(IndexedArrayExpr *expr);

    llvm::Value *emitScalarBoundAE(ScalarBoundAE *expr);
    llvm::Value *emitArrayBoundAE(ArrayBoundAE *expr);

    /// Emits a value representing the lower bound of the given scalar type.
    llvm::Value *emitScalarLowerBound(IntegerType *Ty);

    /// Emits a value representing the upper bound of the given scalar subtype.
    llvm::Value *emitScalarUpperBound(IntegerType *Ty);

    // Conversion emitters.
    llvm::Value *emitCheckedIntegerConversion(Expr *expr, IntegerType *target);

    llvm::Value *lookupDecl(Decl *decl);

    llvm::Value *emitScalarLoad(llvm::Value *ptr);

    llvm::Value *getStackSlot(Decl *decl);

    llvm::Value *createStackSlot(ObjectDecl *decl);

    void associateStackSlot(Decl *decl, llvm::Value *value);

    llvm::Value *lookupBounds(ValueDecl *decl);

    llvm::Value *createBounds(ValueDecl *decl);

    void associateBounds(ValueDecl *decl, llvm::Value *value);

    /// Emits a scalar range check.
    void emitScalarRangeCheck(llvm::Value *sourceVal,
                              IntegerType *sourceTy,
                              IntegerType *targetTy);

    /// Helper method for emitAbstractCall.
    ///
    /// Resolves the target subroutine for an abstract call, given an instance
    /// serving as a formal parameter to a functor, an AbstractDomainDecl \p
    /// abstract and a target subroutine (assumed to be an export of the
    /// abstract domain).
    SubroutineDecl *resolveAbstractSubroutine(DomainInstanceDecl *instance,
                                              AbstractDomainDecl *abstract,
                                              SubroutineDecl *target);

    /// Emits an assertion pragma.
    void emitPragmaAssert(PragmaAssert *pragma);

    std::pair<llvm::Value*, llvm::Value*>
    emitStringLiteral(StringLiteral *expr);

    llvm::Value *computeArrayLength(llvm::Value *bounds);

    void emitArrayCopy(llvm::Value *source, llvm::Value *destination,
                       ArrayType *arrTy);

    void emitArrayCopy(llvm::Value *source, llvm::Value *destination,
                       llvm::Value *bounds);

    std::pair<llvm::Value*, llvm::Value*>
    emitAggregate(AggregateExpr *expr, llvm::Value *dst, bool genTmp);

    llvm::Value *synthAggregateBounds(AggregateExpr *agg);

    void emitArrayConversion(ConversionExpr *convert,
                             llvm::Value *components, llvm::Value *bounds);

    /// Forms X**N via calls to the runtime.
    llvm::Value *emitExponential(llvm::Value *x, llvm::Value *n);

    //===------------------------------------------------------------------===//
    // Helper methods to generate LLVM IR.

    /// \brief Returns an llvm basic block.
    ///
    /// Generates a BasicBlock with the parent taken to be the current
    /// subroutine being generated.
    llvm::BasicBlock *makeBasicBlock(const std::string &name = "",
                                     llvm::BasicBlock *insertBefore = 0) const;

};

} // end comma namespace

#endif
