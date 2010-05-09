//===-- codegen/CodeGenCall.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CGContext.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "DependencySet.h"
#include "SRInfo.h"
#include "comma/ast/AstRewriter.h"
#include "comma/ast/AttribDecl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

namespace {

class CallEmitter {

public:
    CallEmitter(CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder)
        : CGR(CGR),
          CG(CGR.getCodeGen()),
          CGC(CGR.getCGC()),
          CGT(CGC.getCGT()),
          Builder(Builder) { }

    CValue emitSimpleCall(SubroutineCall *call);

    /// Emits a function call using the sret calling convention.
    ///
    /// \param call The function call to emit.  This must be a function
    /// returning a constrained aggregate type.
    ///
    /// \param dst A pointer to storage capable of holding the result of this
    /// call.  If \p dst is null then a temporary is allocated.
    ///
    /// \return Either \p dst or the allocated temporary.
    CValue emitCompositeCall(FunctionCallExpr *call, llvm::Value *dst);

    CValue emitVStackCall(FunctionCallExpr *call);

    void emitProcedureCall(ProcedureCallStmt *call);

private:
    /// The code generation context.
    CodeGenRoutine &CGR;
    CodeGen &CG;
    CGContext &CGC;
    CodeGenTypes &CGT;

    /// The builder we are injecting code into.
    llvm::IRBuilder<> &Builder;

    /// The call expression to emit.
    SubroutineCall *SRCall;

    /// Arguments which are to be supplied to this function call.
    std::vector<llvm::Value*> arguments;

    /// Appends the actual arguments of the callExpr to the arguments vector.
    ///
    /// Note that this method does not generate any implicit first parameter
    /// such as sret return values.
    void emitCallArguments();

    /// Helper method for emitCallArguments.
    ///
    /// Evaluates the given expression with respect to the given type and
    /// parameter mode, appending the resulting Value's to the arguments vector.
    /// Note that two Value's might be generated if the given type is an
    /// unconstrained array.
    ///
    /// \param expr The expression to evaluate.
    ///
    /// \param mode The mode of the associated formal parameter.
    ///
    /// \param type The target type of the parameter, which may be distinct from
    /// the type of the argument.
    void emitArgument(Expr *expr, PM::ParameterMode mode, Type *type);

    /// Helper method for emitArg.
    ///
    /// Evaluates the given expression when the target type is composite.
    ///
    /// \see emitArg()
    void emitCompositeArgument(Expr *expr, PM::ParameterMode mode,
                               CompositeType *type);

    /// Helper method for emitCompositeArgument.
    ///
    /// Evaluates the given expression when the target type is an array type.
    ///
    /// \see emitArg()
    void emitArrayArgument(Expr *expr, PM::ParameterMode mode, ArrayType *type);

    /// Generates a call to a primitive subroutine, returning the computed
    /// result.
    llvm::Value *emitPrimitiveCall();

    /// Generates a call to function provided by an attribute.
    llvm::Value *emitAttributeCall();

    /// \name Primitive Call Emitters.
    //@{

    /// Generates a call into the Comma runtime to handle exponentiation.
    llvm::Value *emitExponential(llvm::Value *x, llvm::Value *n);

    /// Synthesizes a "mod" operation.
    llvm::Value *emitMod(llvm::Value *lhs, llvm::Value *rhs);

    /// Synthesizes a "rem" operation.
    llvm::Value *emitRem(llvm::Value *lhs, llvm::Value *rhs);

    /// Synthesizes a "=" operation.
    llvm::Value *emitEQ(Type *argTy, llvm::Value *lhs, llvm::Value *rhs);

    /// Synthesizes a "/=" operation.
    llvm::Value *emitNE(Type *argTy, llvm::Value *lhs, llvm::Value *rhs);
    //@}

    /// \name Attribute Call Emitters.
    //@{
    llvm::Value *emitAttribute(PosAD *attrib);
    llvm::Value *emitAttribute(ValAD *attrib);
    //@}

    /// Generates any implicit first arguments for the current call expression
    /// and resolves the associated SRInfo object.
    SRInfo *prepareCall();

    SRInfo *prepareLocalCall();
    SRInfo *prepareForeignCall();
    SRInfo *prepareDirectCall();
    SRInfo *prepareAbstractCall();

    /// Helper method for prepareAbstractCall.
    ///
    /// Given an abstract domain \p abstract and a subroutine \p target provided
    /// by the domain, resolve the corresponding declaration in \p instance
    /// (which must be a fully resolved (non-dependent) instance corresponding
    /// to the given abstract domain).
    SubroutineDecl *
    resolveAbstractSubroutine(DomainInstanceDecl *instance,
                              AbstractDomainDecl *abstract,
                              SubroutineDecl *target);

    /// Applies the arguments to the given function.  Synthesizes either a call
    /// or invoke instruction depending on if the current context is handled or
    /// not.
    llvm::Value *emitCall(llvm::Function *fn);

    /// Access to the current frame.
    Frame *frame() { return CGR.getFrame(); }
};

llvm::Value *CallEmitter::emitCall(llvm::Function *fn)
{
    llvm::Value *result;

    if (frame()->hasLandingPad()) {
        llvm::BasicBlock *mergeBB = frame()->makeBasicBlock();
        result = Builder.CreateInvoke(fn, mergeBB, frame()->getLandingPad(),
                                      arguments.begin(), arguments.end());
        Builder.SetInsertPoint(mergeBB);
    }
    else
        result = Builder.CreateCall(fn, arguments.begin(), arguments.end());
    return result;
}

CValue CallEmitter::emitSimpleCall(SubroutineCall *call)
{
    SRCall = call;

    // Directly emit primitive operations.
    if (SRCall->isPrimitive())
        return CValue::get(emitPrimitiveCall());

    // Similarly for attribute functions.
    if (SRCall->isAttributeCall())
        return CValue::get(emitAttributeCall());

    // Prepare any implicit parameters and resolve the SRInfo corresponding to
    // the call.
    SRInfo *callInfo = prepareCall();
    assert(!callInfo->hasSRet() && "Not a simple call!");

    // Generate the actual arguments.
    emitCallArguments();

    // Synthesize the actual call instruction.
    return CValue::get(emitCall(callInfo->getLLVMFunction()));
}

CValue CallEmitter::emitCompositeCall(FunctionCallExpr *call, llvm::Value *dst)
{
    SRCall = call;
    const Type *callTy = CGT.resolveType(call->getType());

    // If the destination is null allocate a temporary.
    if (dst == 0) {
        const llvm::Type *retTy = CGT.lowerType(callTy);
        dst = frame()->createTemp(retTy);
    }

    // Push the destination pointer onto the argument vector.  SRet convention
    // requires the return structure to appear before any implicit arguments.
    arguments.push_back(dst);

    // Prepare any implicit parameters and resolve the SRInfo corresponding to
    // the call.
    SRInfo *callInfo = prepareCall();
    assert(callInfo->hasSRet() && "Not a composite call!");

    // Generate the actual arguments.
    emitCallArguments();

    // Synthesize the actual call instruction.
    emitCall(callInfo->getLLVMFunction());

    if (callTy->isFatAccessType())
        return CValue::getFat(dst);
    else
        return CValue::get(dst);
}

CValue CallEmitter::emitVStackCall(FunctionCallExpr *call)
{
    const CommaRT &CRT = CG.getRuntime();
    SRCall = call;

    // Prepare any implicit parameters and resolve the SRInfo corresponding to
    // the call.
    SRInfo *callInfo = prepareCall();

    // Generate the actual arguments.
    emitCallArguments();

    // Synthesize the actual call instruction.
    emitCall(callInfo->getLLVMFunction());

    // Emit a temporary to hold the bounds.
    ArrayType *arrTy = cast<ArrayType>(CGR.resolveType(call->getType()));
    const llvm::Type *boundsTy = CGT.lowerArrayBounds(arrTy);
    const llvm::Type *boundsPtrTy = CG.getPointerType(boundsTy);
    llvm::Value *boundsSlot = frame()->createTemp(boundsTy);
    llvm::Value *dataSlot;
    llvm::Value *vstack;
    llvm::Value *bounds;

    // Cast the top of the vstack to the bounds type and store it in the
    // temporary.  Pop the stack.
    vstack = CRT.vstack(Builder, boundsPtrTy);
    bounds = Builder.CreateLoad(vstack);
    Builder.CreateStore(bounds, boundsSlot);
    CRT.vstack_pop(Builder);

    // Compute the length of the returned array, mark the frame as stacksave,
    // and allocate a temporary slot for the result.
    BoundsEmitter emitter(CGR);
    frame()->stacksave();
    const llvm::Type *componentTy = CGT.lowerType(arrTy->getComponentType());
    llvm::Value *length = emitter.computeTotalBoundLength(Builder, bounds);
    dataSlot = Builder.CreateAlloca(componentTy, length);

    // Copy the vstack data into the temporary and pop the vstack.
    vstack = CRT.vstack(Builder, CG.getInt8PtrTy());
    CGR.emitArrayCopy(vstack, dataSlot, length, componentTy);
    CRT.vstack_pop(Builder);

    // Cast the data slot to a pointer to VLArray type.
    const llvm::Type *dataTy = CG.getPointerType(CG.getVLArrayTy(componentTy));
    dataSlot = Builder.CreatePointerCast(dataSlot, dataTy);

    // Return the temps.
    return CValue::getArray(dataSlot, boundsSlot);
}

void CallEmitter::emitProcedureCall(ProcedureCallStmt *call)
{
    SRCall = call;

    // Prepare any implicit parameters and resolve the SRInfo corresponding to
    // the call.
    SRInfo *callInfo = prepareCall();
    assert(!callInfo->hasSRet() && "Not a simple call!");

    // Generate the actual arguments.
    emitCallArguments();

    // Synthesize the actual call instruction.
    emitCall(callInfo->getLLVMFunction());
}

void CallEmitter::emitCallArguments()
{
    SubroutineDecl *SRDecl = SRCall->getConnective();

    typedef SubroutineCall::arg_iterator iterator;
    iterator Iter = SRCall->begin_arguments();
    iterator E = SRCall->end_arguments();
    for (unsigned i = 0; Iter != E; ++Iter, ++i) {
        Expr *arg = *Iter;
        PM::ParameterMode mode = SRDecl->getParamMode(i);
        Type *type = SRDecl->getParamType(i);
        emitArgument(arg, mode, type);
    }
}

void CallEmitter::emitArgument(Expr *param, PM::ParameterMode mode, Type *targetTy)
{
    targetTy = CGR.resolveType(targetTy);

    if (CompositeType *compTy = dyn_cast<CompositeType>(targetTy))
        emitCompositeArgument(param, mode, compTy);
    else if (mode == PM::MODE_OUT || mode == PM::MODE_IN_OUT)
        arguments.push_back(CGR.emitReference(param).first());
    else
        arguments.push_back(CGR.emitValue(param).first());
}

void CallEmitter::emitCompositeArgument(Expr *param, PM::ParameterMode mode,
                                        CompositeType *targetTy)
{
    if (ArrayType *arrTy = dyn_cast<ArrayType>(targetTy))
        emitArrayArgument(param, mode, arrTy);
    else {
        // Otherwise we have a record type as target.  Simply push a reference
        // to the record.
        arguments.push_back(CGR.emitCompositeExpr(param, 0, false).first());
    }
}

void CallEmitter::emitArrayArgument(Expr *param, PM::ParameterMode mode,
                                    ArrayType *targetTy)
{
    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(param)) {

        ArrayType *paramTy = cast<ArrayType>(CGR.resolveType(param->getType()));

        if (paramTy->isStaticallyConstrained()) {
            // Perform the function call by allocating a temporary and add the
            // destination to the argument set.
            arguments.push_back(CGR.emitCompositeCall(call, 0).first());

            // If the target type is unconstrained, generate a second temporary
            // stucture to represent the bounds.  Populate with constant values
            // and form the call.
            if (!targetTy->isConstrained()) {
                BoundsEmitter emitter(CGR);
                llvm::Value *bounds;
                bounds = emitter.synthStaticArrayBounds(Builder, paramTy);
                arguments.push_back(bounds);
            }
        }
        else {
            // We do not have dynamically constrained types yet.
            assert(!paramTy->isConstrained());

            // Simply emit the call using the vstack and pass the resulting
            // temporaries to the subroutine.  Do not pass in the bounds if this
            // is a call to an imported subroutine.
            CValue arrValue = CGR.emitVStackCall(call);
            arguments.push_back(arrValue.first());
            if (!targetTy->isConstrained() || !SRCall->isForeignCall())
                arguments.push_back(arrValue.second());
        }
        return;
    }

    // FIXME: Currently, we do not pass arrays by copy (we should).
    CValue arrValue = CGR.emitArrayExpr(param, 0, false);
    llvm::Value *components = arrValue.first();

    if (targetTy->isStaticallyConstrained()) {
        // The target type is statically constrained.  We do not need to emit
        // bounds for the argument in this case.  Simply pass the components.
        arguments.push_back(components);
    }
    else {
        // When the target type is an unconstrained array type, we might need to
        // cast the argument.  Unconstrained arrays are represented as pointers
        // to zero-length LLVM arrays (e.g. [0 x T]*), whereas constrained
        // arrays have a definite dimension.  Lower the target type and cast the
        // argument if necessary.
        const llvm::Type *contextTy;
        contextTy = CGT.lowerArrayType(targetTy);
        contextTy = CG.getPointerType(contextTy);

        if (contextTy != components->getType())
            components = Builder.CreatePointerCast(components, contextTy);

        // Pass the components.  If this subroutine is imported we are finished,
        // otherwise pass the bounds in as well.
        arguments.push_back(components);
        if (!SRCall->isForeignCall()) {
            llvm::Value *bounds = arrValue.second();
            const llvm::Type *boundsTy = bounds->getType();
            if (boundsTy->isAggregateType()) {
                llvm::Value *slot = frame()->createTemp(boundsTy);
                Builder.CreateStore(bounds, slot);
                bounds = slot;
            }
            arguments.push_back(bounds);
        }
    }
}

llvm::Value *CallEmitter::emitPrimitiveCall()
{
    SubroutineDecl *srDecl = SRCall->getConnective();
    PO::PrimitiveID ID = srDecl->getPrimitiveID();
    assert(ID != PO::NotPrimitive && "Not a primitive call!");

    // Primitive subroutines do not accept any implicit parameters, nor follow
    // the sret calling convention.  Populate the argument vector with the
    // values to apply the primitive call to.
    emitCallArguments();

    // Handle the unary and binary cases seperately.
    llvm::Value *result = 0;
    if (PO::denotesUnaryOp(ID)) {
        assert(arguments.size() == 1 && "Arity mismatch!");
        llvm::Value *arg = arguments[0];

        switch (ID) {
        default:
            assert(false && "Cannot codegen primitive!");
            break;

        case PO::POS_op:
            result = arg;       // POS is a no-op.
            break;

        case PO::NEG_op:
            result = Builder.CreateNeg(arg);
            break;

        case PO::LNOT_op:
            result = Builder.CreateNot(arg);
            break;
        }
    }
    else if (PO::denotesBinaryOp(ID)) {
        assert(arguments.size() == 2 && "Arity mismatch!");

        Type *argTy = srDecl->getParamType(0);
        llvm::Value *lhs = arguments[0];
        llvm::Value *rhs = arguments[1];

        switch (ID) {
        default:
            assert(false && "Cannot codegen primitive!");
            break;

        case PO::EQ_op:
            result = emitEQ(argTy, lhs, rhs);
            break;

        case PO::NE_op:
            result = emitNE(argTy, lhs, rhs);
            break;

        case PO::ADD_op:
            result = Builder.CreateAdd(lhs, rhs);
            break;

        case PO::SUB_op:
            result = Builder.CreateSub(lhs, rhs);
            break;

        case PO::MUL_op:
            result = Builder.CreateMul(lhs, rhs);
            break;

        case PO::DIV_op:
            // FIXME: Check for division by zero.
            result = Builder.CreateSDiv(lhs, rhs);
            break;

        case PO::MOD_op:
            result = emitMod(lhs, rhs);
            break;

        case PO::REM_op:
            result = emitRem(lhs, rhs);
            break;

        case PO::POW_op:
            result = emitExponential(lhs, rhs);
            break;

        case PO::LT_op:
            result = Builder.CreateICmpSLT(lhs, rhs);
            break;

        case PO::GT_op:
            result = Builder.CreateICmpSGT(lhs, rhs);
            break;

        case PO::LE_op:
            result = Builder.CreateICmpSLE(lhs, rhs);
            break;

        case PO::GE_op:
            result = Builder.CreateICmpSGE(lhs, rhs);
            break;

        case PO::LOR_op:
            result = Builder.CreateOr(lhs, rhs);
            break;

        case PO::LAND_op:
            result = Builder.CreateAnd(lhs, rhs);
            break;

        case PO::LXOR_op:
            result = Builder.CreateXor(lhs, rhs);
            break;
        }
    }
    else {
        // Currently, there is only one nullary primitive operation.
        assert(ID == PO::ENUM_op && "Cannot codegen primitive!");
        assert(arguments.size() == 0 && "Arity mismatch!");

        EnumLiteral *lit = cast<EnumLiteral>(srDecl);
        unsigned idx = lit->getIndex();
        const llvm::Type *ty = CGT.lowerType(lit->getReturnType());
        result = llvm::ConstantInt::get(ty, idx);
    }
    return result;
}

llvm::Value *CallEmitter::emitAttributeCall()
{
    FunctionAttribDecl *attrib = cast<FunctionAttribDecl>(SRCall->getConnective());
    llvm::Value *result;

    switch (attrib->getKind()) {
    default:
        assert(false && "Unexpected attribute kind!");
        result = 0;
        break;

    case Ast::AST_PosAD:
        result = emitAttribute(cast<PosAD>(attrib));
        break;

    case Ast::AST_ValAD:
        result = emitAttribute(cast<ValAD>(attrib));
        break;

    };

    return result;
}

llvm::Value *CallEmitter::emitAttribute(PosAD *attrib)
{
    BoundsEmitter emitter(CGR);

    // The return type is of type universal_integer.  Perform the following
    // calculations in this domain (this is always the widest integer type
    // provided by the target).
    UniversalType *retTy;
    const llvm::IntegerType *targetTy;
    retTy = cast<UniversalType>(attrib->getReturnType());
    targetTy = cast<llvm::IntegerType>(CGT.lowerUniversalType(retTy));

    Expr *argExpr = *SRCall->begin_arguments();
    DiscreteType *argTy = cast<DiscreteType>(argExpr->getType());
    llvm::Value *arg = CGR.emitValue(argExpr).first();
    const llvm::Type *sourceTy = cast<llvm::IntegerType>(arg->getType());

    // Compute the bounds of the type.
    llvm::Value *lower = emitter.getLowerBound(Builder, argTy);

    // Convert to the target type if needed.
    if (sourceTy != targetTy) {
        if (argTy->isSigned()) {
            arg = Builder.CreateSExt(arg, targetTy);
            lower = Builder.CreateSExt(lower, targetTy);
        }
        else {
            arg = Builder.CreateZExt(arg, targetTy);
            lower = Builder.CreateZExt(lower, targetTy);
        }
    }

    // Subtract the argument from the lower bound.
    return Builder.CreateSub(arg, lower);
}

llvm::Value *CallEmitter::emitAttribute(ValAD *attrib)
{
    BoundsEmitter emitter(CGR);

    // FIXME: The argument is of type universal_integer.  The type checker does
    // not guarantee that this value is within the bounds of the return type.
    // Emit a range check.
    Expr *argExpr = *SRCall->begin_arguments();
    llvm::Value *arg = CGR.emitValue(argExpr).first();

    DiscreteType *returnTy = cast<DiscreteType>(attrib->getReturnType());
    const llvm::Type *targetTy = CGT.lowerType(returnTy);

    // Truncate to the target type if needed.
    if (arg->getType() != targetTy)
        arg = Builder.CreateTrunc(arg, targetTy);

    // Compute the bounds of the type and add the argument to the lower bound.
    llvm::Value *lower = emitter.getLowerBound(Builder, returnTy);
    return Builder.CreateAdd(arg, lower);
}

llvm::Value *CallEmitter::emitExponential(llvm::Value *x, llvm::Value *n)
{
    CommaRT &CRT = CG.getRuntime();

    // Depending on the width of the operands, call into a runtime routine to
    // perform the operation.  Note the the power we raise to is always an i32.
    const llvm::IntegerType *type = cast<llvm::IntegerType>(x->getType());
    const llvm::IntegerType *i32Ty = CG.getInt32Ty();
    const llvm::IntegerType *i64Ty = CG.getInt64Ty();
    unsigned width = type->getBitWidth();
    llvm::Value *result;

    assert(cast<llvm::IntegerType>(n->getType()) == i32Ty &&
           "Unexpected type for rhs of exponential!");

    // Call into the runtime and truncate the results back to the original
    // width.
    if (width < 32) {
        x = Builder.CreateSExt(x, i32Ty);
        result = CRT.pow_i32_i32(Builder, x, n);
        result = Builder.CreateTrunc(result, type);
    }
    else if (width == 32)
        result = CRT.pow_i32_i32(Builder, x, n);
    else if (width < 64) {
        x = Builder.CreateSExt(x, i64Ty);
        result = CRT.pow_i64_i32(Builder, x, n);
        result = Builder.CreateTrunc(result, type);
    }
    else {
        assert(width == 64 && "Integer type too wide!");
        result = CRT.pow_i64_i32(Builder, x, n);
    }

    return result;
}

llvm::Value *CallEmitter::emitMod(llvm::Value *lhs, llvm::Value *rhs)
{
    // FIXME: Raise an exception if rhs is zero.
    //
    // Build:
    //
    //   R := lhs rem rhs;
    //   if equal_sign(lhs, rhs) or (R = 0)
    //      return R;
    //   else
    //      return rhs + R;
    llvm::Value *rem = Builder.CreateSRem(lhs, rhs);
    llvm::Value *zero = llvm::ConstantInt::get(lhs->getType(), 0);
    llvm::Value *lhsNeg = Builder.CreateICmpSLT(lhs, zero);
    llvm::Value *rhsNeg = Builder.CreateICmpSLT(rhs, zero);
    llvm::Value *remZro = Builder.CreateICmpEQ(rem, zero);
    llvm::Value *negOps = Builder.CreateICmpEQ(lhsNeg, rhsNeg);
    llvm::Value *pred = Builder.CreateOr(negOps, remZro);

    return Builder.CreateSelect(pred, rem, Builder.CreateAdd(rhs, rem));
}

llvm::Value *CallEmitter::emitRem(llvm::Value *lhs, llvm::Value *rhs)
{
    // FIXME: Raise an exception if rhs is zero.
    return Builder.CreateSRem(lhs, rhs);
}

llvm::Value *CallEmitter::emitEQ(Type *argTy,
                                 llvm::Value *lhs, llvm::Value *rhs)
{
    if (argTy->isFatAccessType()) {
        // Fat access types are compared wrt the value of the embedded pointer.
        lhs = Builder.CreateLoad(Builder.CreateStructGEP(lhs, 0));
        rhs = Builder.CreateLoad(Builder.CreateStructGEP(rhs, 0));
        lhs = Builder.CreatePtrToInt(lhs, CG.getIntPtrTy());
        rhs = Builder.CreatePtrToInt(rhs, CG.getIntPtrTy());
    }
    else if (argTy->isThinAccessType()) {
        // Cast thin pointer types to the corresponding integer value.
        lhs = Builder.CreatePtrToInt(lhs, CG.getIntPtrTy());
        rhs = Builder.CreatePtrToInt(rhs, CG.getIntPtrTy());
    }

    return Builder.CreateICmpEQ(lhs, rhs);
}

llvm::Value *CallEmitter::emitNE(Type *argTy, llvm::Value *lhs, llvm::Value *rhs)
{
    if (argTy->isFatAccessType()) {
        // Fat access types are compared wrt the value of the embedded pointer.
        lhs = Builder.CreateLoad(Builder.CreateStructGEP(lhs, 0));
        rhs = Builder.CreateLoad(Builder.CreateStructGEP(rhs, 0));
        lhs = Builder.CreatePtrToInt(lhs, CG.getIntPtrTy());
        rhs = Builder.CreatePtrToInt(rhs, CG.getIntPtrTy());
    }
    else if (argTy->isThinAccessType()) {
        // Cast thin pointer types to the corresponding integer value.
        lhs = Builder.CreatePtrToInt(lhs, CG.getIntPtrTy());
        rhs = Builder.CreatePtrToInt(rhs, CG.getIntPtrTy());
    }

    return Builder.CreateICmpNE(lhs, rhs);
}

SRInfo *CallEmitter::prepareCall()
{
    if (SRCall->isForeignCall())
        return prepareForeignCall();
    else if (SRCall->isLocalCall())
        return prepareLocalCall();
    else if (SRCall->isDirectCall())
        return prepareDirectCall();
    else if (SRCall->isAbstractCall())
        return prepareAbstractCall();
    else {
        assert(false && "Unsupported call type!");
        return 0;
    }
}

SRInfo *CallEmitter::prepareLocalCall()
{
    // Resolve the info structure for called subroutine.
    SubroutineDecl *srDecl = SRCall->getConnective();
    InstanceInfo *IInfo = CGC.getInstanceInfo();

    if (DomainInstanceDecl *DID = IInfo->getDomainInstanceDecl())
        return CGR.getCodeGen().getSRInfo(DID, srDecl);
    else {
        PkgInstanceDecl *PID = IInfo->getPkgInstanceDecl();
        return CGR.getCodeGen().getSRInfo(PID, srDecl);
    }
}

SRInfo *CallEmitter::prepareForeignCall()
{
    // Resolve the instance for the given declaration.
    SubroutineDecl *srDecl = SRCall->getConnective();
    DeclRegion *region = srDecl->getDeclRegion();

    if (DomainInstanceDecl *instance = dyn_cast<DomainInstanceDecl>(region))
        return CG.getSRInfo(instance, srDecl);

    if (PkgInstanceDecl *instance = dyn_cast<PkgInstanceDecl>(region))
        return CG.getSRInfo(instance, srDecl);

    // If the given subroutine was not declared in the public interface of a
    // domain or package instance, then it must correspond to an internal
    // declaration.  Resolve with respect to the instance currently being
    // codegened.
    InstanceInfo *iinfo = CGC.getInstanceInfo();
    if (DomainInstanceDecl *instance = iinfo->getDomainInstanceDecl())
        return CG.getSRInfo(instance, srDecl);
    return CG.getSRInfo(iinfo->getPkgInstanceDecl(), srDecl);
}

SRInfo *CallEmitter::prepareDirectCall()
{
    SubroutineDecl *srDecl = SRCall->getConnective();
    InstanceInfo *IInfo = CGC.getInstanceInfo();
    CapsuleInstance *context;

    if (!(context = dyn_cast<DomainInstanceDecl>(srDecl->getDeclRegion())))
        context = cast<PkgInstanceDecl>(srDecl->getDeclRegion());

    // If the declaration context which provides this call is dependent (meaning
    // that it involves percent or other generic parameters),  rewrite the
    // declaration using the actual arguments supplied to this instance.
    CapsuleInstance *targetInstance;
    SubroutineDecl *targetRoutine;
    if (context->isDependent()) {
        AstRewriter rewriter(CG.getAstResource());

        // If the current capsule is a domain map its percent node to the actual
        // instance being codegened.
        if (IInfo->denotesDomainInstance()) {
            DomainInstanceDecl *instance = IInfo->getDomainInstanceDecl();
            rewriter.addTypeRewrite(instance->getDefinition()->getPercentType(),
                                    instance->getType());
        }

        // Map any generic formal parameters to the actual arguments of this
        // instance.
        const CGContext::ParameterMap &paramMap = CGC.getParameterMap();
        rewriter.addTypeRewrites(paramMap.begin(), paramMap.end());

        // If the context providing this call is a domain rewrite its type
        // according to the above rules.
        if (context->denotesDomainInstance()) {
            DomainInstanceDecl *domain = context->asDomainInstance();
            DomainType *targetTy = rewriter.rewriteType(domain->getType());
            targetInstance = targetTy->getInstanceDecl();

            // Extend the rewriter with a rule to map the type of the context
            // domain to the targetInstance.
            rewriter.addTypeRewrite(domain->getType(), targetTy);
        }

        // Rewrite the type of the subroutine and perform a lookup in the target
        // instance.
        SubroutineType *srTy = rewriter.rewriteType(srDecl->getType());
        DeclRegion *region = targetInstance->asDeclRegion();
        Decl *resolvedDecl = region->findDecl(srDecl->getIdInfo(), srTy);
        targetRoutine = cast<SubroutineDecl>(resolvedDecl);
    }
    else {
        targetInstance = context;
        targetRoutine = srDecl;
    }

    // Add the target instance to the code generators worklist.  This will
    // generate SRInfo objects for this particular instance if they do not
    // already exist and schedual the associated functions for generation.
    CG.extendWorklist(targetInstance);

    // Lookup the corresponding SRInfo object.
    return CG.getSRInfo(targetInstance, targetRoutine);
}

SRInfo *CallEmitter::prepareAbstractCall()
{
    // Resolve the abstract domain declaration providing this call.
    SubroutineDecl *srDecl = SRCall->getConnective();
    AbstractDomainDecl *abstract =
        cast<AbstractDomainDecl>(srDecl->getDeclRegion());

    // Resolve the abstract domain to a concrete type using the parameter map
    // provided by the capsule context.
    DomainInstanceDecl *instance = CGC.rewriteAbstractDecl(abstract);
    assert(instance && "Failed to resolve abstract domain!");

    // Add this instance to the code generators worklist, thereby ensuring
    // forward declarations are generated and that the implementation will be
    // codegened.
    CG.extendWorklist(instance);

    // Resolve the needed routine.
    SubroutineDecl *resolvedRoutine;
    resolvedRoutine = resolveAbstractSubroutine(instance, abstract, srDecl);

     // Lookup the associated SRInfo and return the llvm function.
    return CG.getSRInfo(instance, resolvedRoutine);
}

SubroutineDecl *
CallEmitter::resolveAbstractSubroutine(DomainInstanceDecl *instance,
                                       AbstractDomainDecl *abstract,
                                       SubroutineDecl *target)
{
    DomainType *abstractTy = abstract->getType();

    // The instance must provide a subroutine declaration with a type matching
    // that of the original, with the only exception being that occurrences of
    // abstractTy are mapped to the type of the given instance.  Use an
    // AstRewriter to obtain the required target type.
    AstRewriter rewriter(CGR.getCodeGen().getAstResource());
    rewriter.addTypeRewrite(abstractTy, instance->getType());
    SubroutineType *targetTy = rewriter.rewriteType(target->getType());

    // Lookup the target declaration directly in the instance.
    Decl *resolvedDecl = instance->findDecl(target->getIdInfo(), targetTy);
    return cast<SubroutineDecl>(resolvedDecl);
}

} // end anonymous namespace.

CValue CodeGenRoutine::emitFunctionCall(FunctionCallExpr *expr)
{
    CallEmitter emitter(*this, Builder);

    if (resolveType(expr)->isFatAccessType())
        return emitter.emitCompositeCall(expr, 0);
    else
        return emitter.emitSimpleCall(expr);
}

CValue CodeGenRoutine::emitSimpleCall(FunctionCallExpr *expr)
{
    CallEmitter emitter(*this, Builder);
    return emitter.emitSimpleCall(expr);
}

CValue CodeGenRoutine::emitCompositeCall(FunctionCallExpr *expr,
                                         llvm::Value *dst)
{
    CallEmitter emitter(*this, Builder);
    return emitter.emitCompositeCall(expr, dst);
}

CValue CodeGenRoutine::emitVStackCall(FunctionCallExpr *expr)
{
    CallEmitter emitter(*this, Builder);
    return emitter.emitVStackCall(expr);
}

void CodeGenRoutine::emitProcedureCallStmt(ProcedureCallStmt *stmt)
{
    CallEmitter emitter(*this, Builder);
    emitter.emitProcedureCall(stmt);
}
