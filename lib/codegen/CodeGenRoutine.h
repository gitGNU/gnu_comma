//===-- codegen/CodeGenRoutine.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENROUTINE_HDR_GUARD
#define COMMA_CODEGEN_CODEGENROUTINE_HDR_GUARD

#include "CodeGen.h"
#include "CValue.h"
#include "Frame.h"
#include "comma/ast/Expr.h"

#include "llvm/DerivedTypes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/IRBuilder.h"

namespace llvm {

class BasicBlock;
class Function;

} // end namespace llvm;

namespace comma {

class CGContext;

// This class provides for code generation of subroutines.
class CodeGenRoutine {

    CodeGen       &CG;
    CGContext     &CGC;
    CodeGenTypes  &CGT;
    const CommaRT &CRT;

    // The info node for the subroutine we are emitting code for.
    SRInfo *SRI;

    // Builder object used to construct LLVM IR.
    llvm::IRBuilder<> Builder;

    // Frame encapsulating this functions IR.
    SRFrame *SRF;

public:
    CodeGenRoutine(CGContext &CGC, SRInfo *info);

    /// Returns the code generator.
    CodeGen &getCodeGen() { return CG; }

    /// Returns the code generator context.
    CGContext &getCGC() { return CGC; }

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

    CValue emitValue(Expr *expr);
    CValue emitReference(Expr *expr);

    CValue emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp);

    CValue emitRecordExpr(Expr *expr, llvm::Value *dst, bool genTmp);

    CValue emitCompositeExpr(Expr *expr, llvm::Value *dst, bool genTmp);

    void emitStmt(Stmt *stmt);

    CValue emitSimpleCall(FunctionCallExpr *expr);
    CValue emitFunctionCall(FunctionCallExpr *expr);

    /// Emits a function call using the sret calling convention.
    ///
    /// \param call The function call to emit.  This must be a function
    /// returning a constrained aggregate type.
    ///
    /// \param dst A pointer to storage capable of holding the result of this
    /// call.  If \p dst is null then a temporary is allocated.
    ///
    /// \return A CValue containing \p dst or the allocated temporary.
    CValue emitCompositeCall(FunctionCallExpr *expr, llvm::Value *dst);

    CValue emitVStackCall(FunctionCallExpr *expr);

    void emitArrayCopy(llvm::Value *source, llvm::Value *destination,
                       ArrayType *arrTy);

    void emitArrayCopy(llvm::Value *source, llvm::Value *destination,
                       llvm::Value *length, const llvm::Type *componentTy);

    CValue emitIndexedArrayRef(IndexedArrayExpr *expr);
    CValue emitSelectedRef(SelectedExpr *expr);

    Type *resolveType(Type *type);
    Type *resolveType(Expr *expr) {
        return resolveType(expr->getType());
    }

    /// Emits a null check for the given pointer value.
    void emitNullAccessCheck(llvm::Value *pointer, Location loc);

private:
    // Returns the llvm function we are generating code for.
    llvm::Function *getLLVMFunction() const;

    /// Generates code for the current subroutines body.
    void emitSubroutineBody();

    void emitObjectDecl(ObjectDecl *objDecl);
    void emitRenamedObjectDecl(RenamedObjectDecl *objDecl);

    void emitIfStmt(IfStmt *ite);
    void emitReturnStmt(ReturnStmt *ret);
    void emitVStackReturn(Expr *expr, ArrayType *type);
    void emitStmtSequence(StmtSequence *seq);
    void emitProcedureCallStmt(ProcedureCallStmt *stmt);
    void emitAssignmentStmt(AssignmentStmt *stmt);
    void emitWhileStmt(WhileStmt *stmt);
    void emitForStmt(ForStmt *stmt);
    void emitLoopStmt(LoopStmt *stmt);
    void emitRaiseStmt(RaiseStmt *stmt);
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

    CValue emitDeclRefExpr(DeclRefExpr *expr);
    CValue emitPrjExpr(PrjExpr *expr);
    CValue emitInjExpr(InjExpr *expr);
    CValue emitIntegerLiteral(IntegerLiteral *expr);
    CValue emitIndexedArrayValue(IndexedArrayExpr *expr);
    CValue emitConversionValue(ConversionExpr *expr);
    CValue emitAttribExpr(AttribExpr *expr);
    CValue emitSelectedValue(SelectedExpr *expr);
    CValue emitNullExpr(NullExpr *expr);
    CValue emitDereferencedValue(DereferenceExpr *expr);
    CValue emitAllocatorValue(AllocatorExpr *expr);
    CValue emitCompositeAllocator(AllocatorExpr *expr);

    llvm::Value *emitScalarBoundAE(ScalarBoundAE *expr);
    llvm::Value *emitArrayBoundAE(ArrayBoundAE *expr);

    // Conversion emitters.
    llvm::Value *emitDiscreteConversion(Expr *expr, DiscreteType *target);

    /// Emits a range check over discrete types.
    void emitDiscreteRangeCheck(llvm::Value *sourceVal, Location loc,
                                Type *sourceTy, DiscreteType *targetTy);

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

    void emitCompositeObjectDecl(ObjectDecl *objDecl);

    /// Forms X**N via calls to the runtime.
    llvm::Value *emitExponential(llvm::Value *x, llvm::Value *n);

    /// Returns the lower and upper bounds of the given range attribute.
    std::pair<llvm::Value*, llvm::Value*> emitRangeAttrib(RangeAttrib *attrib);
};

} // end comma namespace

#endif
