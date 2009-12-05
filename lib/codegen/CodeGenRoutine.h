//===-- codegen/CodeGenRoutine.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENROUTINE_HDR_GUARD
#define COMMA_CODEGEN_CODEGENROUTINE_HDR_GUARD

#include "Frame.h"
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

    // The info node for the subroutine we are emitting code for.
    SRInfo *SRI;

    // Builder object used to construct LLVM IR.
    llvm::IRBuilder<> Builder;

    // Frame encapsulating this functions IR.
    SRFrame *SRF;

public:
    CodeGenRoutine(CodeGenCapsule &CGC, SRInfo *info);

    /// Returns the associated code generator context.
    CodeGen &getCodeGen() { return CG; }

    /// Returns the associated capsule generator context.
    CodeGenCapsule &getCGC() { return CGC; }

    /// \brief Returns the SRInfo object corresponding to the subroutine being
    /// generated.
    SRInfo *getSRInfo() { return SRI; }

    /// Returns the SRFrame object corresponding to the subroutine being
    /// generated.
    SRFrame *getSRFrame() { return SRF; }

    llvm::Value *getImplicitContext() const {
        return SRF->getImplicitContext();
    }

    void emit();

    llvm::Value *emitValue(Expr *expr);
    llvm::Value *emitVariableReference(Expr *expr);
    std::pair<llvm::Value*, llvm::Value*>
    emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp);

    llvm::Value *emitSimpleCall(FunctionCallExpr *expr);

    void emitCompositeCall(FunctionCallExpr *expr, llvm::Value *dst);

    std::pair<llvm::Value*, llvm::Value*>
    emitVStackCall(FunctionCallExpr *expr);

    void emitArrayCopy(llvm::Value *source, llvm::Value *destination,
                       ArrayType *arrTy);

    void emitArrayCopy(llvm::Value *source, llvm::Value *destination,
                       llvm::Value *length, const llvm::Type *componentTy);

    PrimaryType *resolveType(Type *type);

private:
    // Returns the llvm function we are generating code for.
    llvm::Function *getLLVMFunction() const;

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
    void emitForStmt(ForStmt *stmt);
    void emitLoopStmt(LoopStmt *stmt);
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
    llvm::Value *emitScalarLowerBound(DiscreteType *Ty);

    /// Emits a value representing the upper bound of the given scalar subtype.
    llvm::Value *emitScalarUpperBound(DiscreteType *Ty);

    // Conversion emitters.
    llvm::Value *emitDiscreteConversion(Expr *expr, DiscreteType *target);

    /// Emits a range check over discrete types.
    void emitDiscreteRangeCheck(llvm::Value *sourceVal,
                                DiscreteType *sourceTy,
                                DiscreteType *targetTy);

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

    std::pair<llvm::Value*, llvm::Value*>
    emitAggregate(AggregateExpr *expr, llvm::Value *dst, bool genTmp);

    std::pair<llvm::Value*, llvm::Value*>
    emitArrayConversion(ConversionExpr *convert, llvm::Value *dst, bool genTmp);

    void emitArrayObjectDecl(ObjectDecl *objDecl);

    void emitIntegerSubtypeDecl(IntegerSubtypeDecl *subDecl);
    void emitEnumSubtypeDecl(EnumSubtypeDecl *subDecl);

    /// Forms X**N via calls to the runtime.
    llvm::Value *emitExponential(llvm::Value *x, llvm::Value *n);

    /// Returns the lower and upper bounds of the given range attribute.
    std::pair<llvm::Value*, llvm::Value*> emitRangeAttrib(RangeAttrib *attrib);

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
