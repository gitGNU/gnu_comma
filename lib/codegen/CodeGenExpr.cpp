//===-- codegen/CodeGenExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenCapsule.h"
#include "DependencySet.h"
#include "SRInfo.h"
#include "comma/ast/AstRewriter.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"
#include "comma/codegen/Mangle.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

llvm::Value *CodeGenRoutine::emitExpr(Expr *expr)
{
    llvm::Value *val = 0;

    switch (expr->getKind()) {
    default:
        if (AttribExpr *attrib = dyn_cast<AttribExpr>(expr))
            val = emitAttribExpr(attrib);
        else {
            assert(false && "Cannot codegen expression!");
            val = 0;
        }
        break;

    case Ast::AST_DeclRefExpr:
        val = emitDeclRefExpr(cast<DeclRefExpr>(expr));
        break;

    case Ast::AST_FunctionCallExpr:
        val = emitFunctionCall(cast<FunctionCallExpr>(expr));
        break;

    case Ast::AST_InjExpr:
        val = emitInjExpr(cast<InjExpr>(expr));
        break;

    case Ast::AST_PrjExpr:
        val = emitPrjExpr(cast<PrjExpr>(expr));
        break;

    case Ast::AST_IntegerLiteral:
        val = emitIntegerLiteral(cast<IntegerLiteral>(expr));
        break;

    case Ast::AST_StringLiteral:
        val = emitStringLiteral(cast<StringLiteral>(expr));
        break;

    case Ast::AST_IndexedArrayExpr:
        val = emitIndexedArrayValue(cast<IndexedArrayExpr>(expr));
        break;

    case Ast::AST_ConversionExpr:
        val = emitConversionValue(cast<ConversionExpr>(expr));
        break;
    }

    return val;
}

llvm::Value *CodeGenRoutine::emitDeclRefExpr(DeclRefExpr *expr)
{
    llvm::Value *val = lookupDecl(expr->getDeclaration());
    assert(val && "DeclRef lookup failed!");
    return val;
}

llvm::Value *CodeGenRoutine::emitFunctionCall(FunctionCallExpr *expr)
{
    assert(expr->isUnambiguous() && "Function call not fully resolved!");
    FunctionDecl *fdecl = expr->getConnective();
    std::vector<llvm::Value *> args;

    typedef FunctionCallExpr::arg_iterator iterator;
    iterator Iter = expr->begin_arguments();
    iterator E = expr->end_arguments();
    for (unsigned i = 0; Iter != E; ++Iter, ++i) {
        Expr *arg = *Iter;
        emitCallArgument(fdecl, arg, i, args);
    }

    if (fdecl->isPrimitive())
        return emitPrimitiveCall(expr, args);
    else if (fdecl->hasPragma(pragma::Import))
        return emitForeignCall(fdecl, args);
    else if (isLocalCall(expr))
        return emitLocalCall(fdecl, args);
    else if (isDirectCall(expr))
        return emitDirectCall(fdecl, args);
    else
        return emitAbstractCall(fdecl, args);
}

llvm::Value *CodeGenRoutine::emitCall(SubroutineType *srTy,
                                      llvm::Value *func,
                                      std::vector<llvm::Value *> &args)
{
    // If the given function follows the struct return convention, allocate a
    // temporary to hold the results.
    if (FunctionType *fnTy = dyn_cast<FunctionType>(srTy)) {
        if (fnTy->getReturnType()->isCompositeType()) {
            const llvm::PointerType *ptrTy;
            const llvm::FunctionType *fnTy;
            ptrTy = cast<llvm::PointerType>(func->getType());
            fnTy = cast<llvm::FunctionType>(ptrTy->getElementType());
            ptrTy = cast<llvm::PointerType>(fnTy->getParamType(0));

            llvm::Value *slot = createTemporary(ptrTy->getElementType());
            args.insert(args.begin(), slot);

            Builder.CreateCall(func, args.begin(), args.end());
            return slot;
        }
    }
    return Builder.CreateCall(func, args.begin(), args.end());
}

void CodeGenRoutine::emitCallArgument(SubroutineDecl *srDecl, Expr *expr,
                                      unsigned argPosition,
                                      std::vector<llvm::Value *> &args)
{
    Type *paramTy = srDecl->getParamType(argPosition);
    PM::ParameterMode mode = srDecl->getParamMode(argPosition);
    llvm::Value *arg;

    if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
        arg = emitVariableReference(expr);
    else
        arg = emitValue(expr);

    if (ArrayType *arrTy = dyn_cast<ArrayType>(paramTy)) {
        // When the expected type is an unconstrained array type, we might need
        // to cast the argument.  Unconstrained arrays are represented as
        // pointers to empty LLVM arrays (e.g. [0 x T]*), whereas constrained
        // arrays have a definite dimension.  Lower the target type and cast the
        // argument if necessary.
        const llvm::Type *contextTy;
        contextTy = CGT.lowerType(srDecl->getParamType(argPosition));
        contextTy = CG.getPointerType(contextTy);

        if (contextTy != arg->getType())
            arg = Builder.CreatePointerCast(arg, contextTy);

        args.push_back(arg);

        // Emit the bounds.
        if (!arrTy->isConstrained())
            args.push_back(emitArrayBounds(expr));
    }
    else
        args.push_back(arg);
}

llvm::Value *CodeGenRoutine::emitLocalCall(SubroutineDecl *srDecl,
                                           std::vector<llvm::Value *> &args)
{
    // Insert the implicit first parameter, which for a local call is the
    // percent handed to the current subroutine.
    args.insert(args.begin(), percent);

    // Get the info structure for this declaration using the current instance as
    // context.
    DomainInstanceDecl *instance = CGC.getInstance();
    SRInfo *info = CG.getSRInfo(instance, srDecl);
    llvm::Value *func = info->getLLVMFunction();

    return emitCall(srDecl->getType(), func, args);
}

llvm::Value *CodeGenRoutine::emitForeignCall(SubroutineDecl *srDecl,
                                             std::vector<llvm::Value*> &args)
{
    // Resolve the instance for the given declaration.
    DeclRegion *region = srDecl->getDeclRegion();
    DomainInstanceDecl *instance = dyn_cast<DomainInstanceDecl>(region);

    if (!instance)
        instance = CGC.getInstance();

    SRInfo *info = CG.getSRInfo(instance, srDecl);
    llvm::Value *func = info->getLLVMFunction();
    return emitCall(srDecl->getType(), func, args);
}

llvm::Value *CodeGenRoutine::emitDirectCall(SubroutineDecl *srDecl,
                                            std::vector<llvm::Value *> &args)
{
    DomainInstanceDecl *instance
        = cast<DomainInstanceDecl>(srDecl->getDeclRegion());

    // Get the dependency ID for this instance and index into percent to obtain
    // the appropriate domain_instance runtime object.
    const DependencySet &DSet = CG.getDependencySet(CGC.getCapsule());
    DependencySet::iterator IDPos = DSet.find(instance);
    assert(IDPos != DSet.end() && "Failed to resolve dependency!");
    unsigned instanceID = DSet.getDependentID(IDPos);
    args.insert(args.begin(), CRT.getLocalCapsule(Builder, percent, instanceID));

    AstRewriter rewriter(CG.getAstResource());
    // Always map percent nodes from the current capsule to the instance.
    rewriter.addTypeRewrite(CGC.getCapsule()->getPercentType(),
                            CGC.getInstance()->getType());

    // If the instance is dependent on formal parameters, rewrite using the
    // current parameter map.
    if (instance->isDependent()) {
        const CodeGenCapsule::ParameterMap &paramMap = CGC.getParameterMap();
        rewriter.addTypeRewrites(paramMap.begin(), paramMap.end());
    }

    DomainType *targetDomainTy = rewriter.rewriteType(instance->getType());
    DomainInstanceDecl *targetInstance = targetDomainTy->getInstanceDecl();
    assert(!targetInstance->isDependent() &&
           "Instance rewriting did not remove all dependencies!");

    // Add the target domain to the code generators worklist.  This will
    // generate forward declarations for the type if they do not already
    // exist.
    CG.extendWorklist(targetInstance);

    // Extend the rewriter with rules mapping the dependent instance type to
    // the rewritten type.  Using this extended rewriter, get the concrete
    // type for the subroutine decl we are calling, and locate it in the
    // target instance (this operation must not fail).
    rewriter.addTypeRewrite(instance->getType(), targetDomainTy);
    SubroutineType *targetTy = rewriter.rewriteType(srDecl->getType());
    srDecl = dyn_cast_or_null<SubroutineDecl>(
        targetInstance->findDecl(srDecl->getIdInfo(), targetTy));
    assert(srDecl && "Failed to resolve subroutine!");

    // With our fully-resolved subroutine, get the corresponding SRInfo object
    // and LLVM function.
    SRInfo *info = CG.getSRInfo(targetInstance, srDecl);
    llvm::Value *func = info->getLLVMFunction();
    return emitCall(srDecl->getType(), func, args);
}

llvm::Value *CodeGenRoutine::emitPrimitiveCall(FunctionCallExpr *expr,
                                               std::vector<llvm::Value *> &args)
{
    assert(expr->isUnambiguous() && "Function call not fully resolved!");
    FunctionDecl *decl = expr->getConnective();

    switch (decl->getPrimitiveID()) {

    default:
        assert(false && "Cannot codegen primitive!");
        return 0;

    case PO::EQ_op:
        return Builder.CreateICmpEQ(args[0], args[1]);

    case PO::ADD_op:
        return Builder.CreateAdd(args[0], args[1]);

    case PO::SUB_op:
        return Builder.CreateSub(args[0], args[1]);

    case PO::MUL_op:
        return Builder.CreateMul(args[0], args[1]);

    case PO::POW_op:
        return emitExponential(args[0], args[1]);

    case PO::POS_op:
        return args[0];         // POS is a no-op.

    case PO::NEG_op:
        return Builder.CreateNeg(args[0]);

    case PO::LT_op:
        return Builder.CreateICmpSLT(args[0], args[1]);

    case PO::GT_op:
        return Builder.CreateICmpSGT(args[0], args[1]);

    case PO::LE_op:
        return Builder.CreateICmpSLE(args[0], args[1]);

    case PO::GE_op:
        return Builder.CreateICmpSGE(args[0], args[1]);

    case PO::ENUM_op: {
        EnumLiteral *lit = cast<EnumLiteral>(decl);
        unsigned idx = lit->getIndex();
        const llvm::Type *ty = CGT.lowerType(lit->getReturnType());
        return llvm::ConstantInt::get(ty, idx);
    }
    };
}

llvm::Value *CodeGenRoutine::emitExponential(llvm::Value *x, llvm::Value *n)
{
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

llvm::Value *CodeGenRoutine::emitAbstractCall(SubroutineDecl *srDecl,
                                              std::vector<llvm::Value *> &args)
{
    // Resolve the region for this declaration, which is asserted to be an
    // AbstractDomainDecl.
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

    // Resolve the needed routine.  This must not fail.
    SubroutineDecl *resolvedRoutine
        = resolveAbstractSubroutine(instance, abstract, srDecl);
    assert(resolvedRoutine && "Failed to resolve abstract subroutine!");

    // Index into percent to obtain the actual domain instance serving as a
    // parameter.
    FunctorDecl *functor = cast<FunctorDecl>(CGC.getCapsule());
    unsigned index = functor->getFormalIndex(abstract);
    llvm::Value *DOC = CRT.getCapsuleParameter(Builder, percent, index);

    // Insert the DOC as the implicit first parameter for the call, obtain the
    // actual llvm function representing the instances subroutine, and emit the
    // call.
    args.insert(args.begin(), DOC);
    SRInfo *info = CG.getSRInfo(instance, resolvedRoutine);
    llvm::Value *func = info->getLLVMFunction();
    assert(func && "function lookup failed!");
    return emitCall(resolvedRoutine->getType(), func, args);
}

SubroutineDecl *
CodeGenRoutine::resolveAbstractSubroutine(DomainInstanceDecl *instance,
                                          AbstractDomainDecl *abstract,
                                          SubroutineDecl *target)
{
    DomainType *abstractTy = abstract->getType();

    // The instance must provide a subroutine declaration with a type matching
    // that of the original, with the only exception being that occurrences of
    // abstractTy are mapped to the type of the given instance.  Use an
    // AstRewriter to obtain the required target type.
    AstRewriter rewriter(CG.getAstResource());
    rewriter.addTypeRewrite(abstractTy, instance->getType());
    SubroutineType *targetTy = rewriter.rewriteType(target->getType());

    // Lookup the target declaration directly in the instance.
    return dyn_cast_or_null<SubroutineDecl>(
        instance->findDecl(target->getIdInfo(), targetTy));
}

llvm::Value *CodeGenRoutine::emitInjExpr(InjExpr *expr)
{
    return emitValue(expr->getOperand());
}

llvm::Value *CodeGenRoutine::emitPrjExpr(PrjExpr *expr)
{
    return emitValue(expr->getOperand());
}

llvm::Value *CodeGenRoutine::emitIntegerLiteral(IntegerLiteral *expr)
{
    assert(expr->hasType() && "Unresolved literal type!");

    const llvm::IntegerType *ty =
        cast<llvm::IntegerType>(CGT.lowerType(expr->getType()));
    llvm::APInt val(expr->getValue());

    // All comma integer literals are represented as signed APInt's.  Sign
    // extend the value if needed to fit in the representation type.
    unsigned valWidth = val.getBitWidth();
    unsigned tyWidth = ty->getBitWidth();
    assert(valWidth <= tyWidth && "Value/Type width mismatch!");

    if (valWidth < tyWidth)
        val.sext(tyWidth);

    return llvm::ConstantInt::get(CG.getLLVMContext(), val);
}

llvm::Value *CodeGenRoutine::emitIndexedArrayRef(IndexedArrayExpr *expr)
{
    assert(expr->getNumIndices() == 1 &&
           "Multidimensional arrays are not yet supported!");

    DeclRefExpr *arrRefExpr = expr->getArrayExpr();
    Expr *idxExpr = expr->getIndex(0);
    llvm::Value *arrValue = lookupDecl(arrRefExpr->getDeclaration());
    llvm::Value *idxValue = emitValue(idxExpr);

    ArrayType *arrType = cast<ArrayType>(arrRefExpr->getType());

    if (arrType->isConstrained()) {
        // Resolve the index type of the array (not the type of the index
        // expression).
        DiscreteType *indexType = arrType->getIndexType(0);

        // If the index type is an integer type adjust the index expression by
        // the lower bound.  If the bound happens to be a constant zero the
        // IRBuilder will fold the ajustment.
        if (IntegerType *intTy = dyn_cast<IntegerType>(indexType)) {
            llvm::Value *adjust = emitScalarLowerBound(intTy);
            idxValue = Builder.CreateSub(idxValue, adjust);
        }
    }
    else {
        // The array expression is unconstrained.  Lookup the bounds for the
        // array and adjust the index if needed.
        ValueDecl *decl = expr->getArrayExpr()->getDeclaration();
        llvm::Value *boundSlot = lookupBounds(decl);
        assert(boundSlot && "Could not retrieve array bounds!");

        // Grab the lower bound and subtract it from the index.
        llvm::Value *bounds = Builder.CreateConstGEP1_32(boundSlot, 0);
        llvm::Value *lower = Builder.CreateStructGEP(bounds, 0);
        llvm::Value *adjust = Builder.CreateLoad(lower);
        idxValue = Builder.CreateSub(idxValue, adjust);
    }

    // Arrays are always represented as pointers to the aggregate. GEP the
    // component.
    llvm::Value *indices[2];
    indices[0] = llvm::ConstantInt::get(CG.getInt32Ty(), (uint64_t)0);
    indices[1] = idxValue;
    return Builder.CreateGEP(arrValue, indices, indices + 2);
}

llvm::Value *CodeGenRoutine::emitIndexedArrayValue(IndexedArrayExpr *expr)
{
    llvm::Value *component = emitIndexedArrayRef(expr);
    return Builder.CreateLoad(component);
}

llvm::Value *CodeGenRoutine::emitConversionValue(ConversionExpr *expr)
{
    // The only type of conversions we currently support are integer
    // conversions.
    if (IntegerType *target = dyn_cast<IntegerType>(expr->getType())) {
        return emitCheckedIntegerConversion(expr->getOperand(), target);
    }

    assert(false && "Cannot codegen given conversion yet!");
    return 0;
}

void
CodeGenRoutine::emitScalarRangeCheck(llvm::Value *sourceVal,
                                     IntegerType *sourceTy,
                                     IntegerType *targetTy)
{
    IntegerType *targetRootTy = targetTy->getRootType();
    IntegerType *sourceRootTy = sourceTy->getRootType();
    const llvm::IntegerType *boundTy;

    // Range checks need to be performed using the larger type (most often the
    // source type).  Find the appropriate type and sign extend the value if
    // needed.
    if (targetRootTy->getSize() > sourceRootTy->getSize()) {
        boundTy = CGT.lowerIntegerType(targetTy);
        sourceVal = Builder.CreateSExt(sourceVal, boundTy);
    }
    else
        boundTy = cast<llvm::IntegerType>(sourceVal->getType());

    llvm::APInt lower;
    llvm::APInt upper;

    // If the target subtype is constrained, extract the bounds of the
    // constraint.  Otherwise, use the bounds of the base type.
    //
    // FIXME: Support dynmaic ranges.
    if (Range *range = targetTy->getConstraint()) {
        assert(range->isStatic() && "Dynamic ranges not supported yet!");
        lower = range->getStaticLowerBound();
        upper = range->getStaticUpperBound();
    }
    else {
        targetRootTy->getLowerLimit(lower);
        targetRootTy->getUpperLimit(upper);
    }

    if (lower.getBitWidth() < boundTy->getBitWidth())
        lower.sext(boundTy->getBitWidth());
    if (upper.getBitWidth() < boundTy->getBitWidth())
        upper.sext(boundTy->getBitWidth());

    // Obtain constants for the bounds.
    llvm::Constant *lowBound = llvm::ConstantInt::get(boundTy, lower);
    llvm::Constant *highBound = llvm::ConstantInt::get(boundTy, upper);

    // Build our basic blocks.
    llvm::BasicBlock *checkHighBB = makeBasicBlock("high.check");
    llvm::BasicBlock *checkFailBB = makeBasicBlock("check.fail");
    llvm::BasicBlock *checkMergeBB = makeBasicBlock("check.merge");

    // Check the low bound.
    llvm::Value *lowPass = Builder.CreateICmpSLE(lowBound, sourceVal);
    Builder.CreateCondBr(lowPass, checkHighBB, checkFailBB);

    // Check the high bound.
    Builder.SetInsertPoint(checkHighBB);
    llvm::Value *highPass = Builder.CreateICmpSLE(sourceVal, highBound);
    Builder.CreateCondBr(highPass, checkMergeBB, checkFailBB);

    // Raise an exception if the check failed.
    Builder.SetInsertPoint(checkFailBB);
    llvm::GlobalVariable *msg = CG.emitInternString("Range check failed!");
    CRT.raise(Builder, msg);

    // Switch the context to the success block.
    Builder.SetInsertPoint(checkMergeBB);
}

llvm::Value *
CodeGenRoutine::emitCheckedIntegerConversion(Expr *expr,
                                             IntegerType *targetTy)
{
    IntegerType *sourceTy = cast<IntegerType>(expr->getType());
    unsigned targetWidth = targetTy->getSize();
    unsigned sourceWidth = sourceTy->getSize();

    // Evaluate the source expression.
    llvm::Value *sourceVal = emitValue(expr);

    // If the source and target types are identical, we are done.
    if (sourceTy == targetTy)
        return sourceVal;

    // If the target type contains the source type then a range check is unnessary.
    if (targetTy->contains(sourceTy)) {
        if (targetWidth == sourceWidth)
            return sourceVal;
        if (targetWidth > sourceWidth)
            return Builder.CreateSExt(sourceVal, CGT.lowerType(targetTy));
    }

    emitScalarRangeCheck(sourceVal, sourceTy, targetTy);

    // Truncate/extend the value if needed to the target size.
    if (targetWidth < sourceWidth)
        sourceVal = Builder.CreateTrunc(sourceVal, CGT.lowerType(targetTy));
    if (targetWidth > sourceWidth)
        sourceVal = Builder.CreateSExt(sourceVal, CGT.lowerType(targetTy));
    return sourceVal;
}

llvm::Value *CodeGenRoutine::emitScalarLowerBound(IntegerType *Ty)
{
    const llvm::IntegerType *loweredTy = CGT.lowerIntegerType(Ty);

    // If unconstrained, emit the lower limit of the base type.
    if (!Ty->isConstrained()) {
        llvm::APInt bound;
        Ty->getLowerLimit(bound);
        return CG.getConstantInt(loweredTy, bound);
    }

    Range *range = Ty->getConstraint();

    // Emit a constant if the range has a static lower bound.
    if (range->hasStaticLowerBound()) {
        llvm::APInt bound(range->getStaticLowerBound());
        return CG.getConstantInt(loweredTy, bound);
    }

    // Otherwise, we have a dynamic lower bound.
    return emitValue(range->getLowerBound());
}

llvm::Value *CodeGenRoutine::emitScalarUpperBound(IntegerType *Ty)
{
    const llvm::IntegerType *loweredTy = CGT.lowerIntegerType(Ty);

    // If unconstrained, emit the lower limit of the base type.
    if (!Ty->isConstrained()) {
        llvm::APInt bound;
        Ty->getUpperLimit(bound);
        return CG.getConstantInt(loweredTy, bound);
    }

    Range *range = Ty->getConstraint();

    // Emit a constant if the range has a static lower bound.
    if (range->hasStaticUpperBound()) {
        llvm::APInt bound(range->getStaticUpperBound());
        return CG.getConstantInt(loweredTy, bound);
    }

    // Otherwise, we have a dynamic upper bound.
    return emitValue(range->getUpperBound());
}

llvm::Value *CodeGenRoutine::emitAttribExpr(AttribExpr *expr)
{
    if (ScalarBoundAE *scalarAE = dyn_cast<ScalarBoundAE>(expr))
        return emitScalarBoundAE(scalarAE);

    if (ArrayBoundAE *arrayAE = dyn_cast<ArrayBoundAE>(expr))
        return emitArrayBoundAE(arrayAE);

    assert(false && "Cannot codegen attribute yet!");
    return 0;
}

llvm::Value *CodeGenRoutine::emitScalarBoundAE(ScalarBoundAE *AE)
{
    IntegerType *Ty = AE->getType();
    if (AE->isFirst())
        return emitScalarLowerBound(Ty);
    else
        return emitScalarUpperBound(Ty);
}

llvm::Value *CodeGenRoutine::emitArrayBoundAE(ArrayBoundAE *AE)
{
    ArrayType *arrTy = AE->getPrefixType();

    if (arrTy->isConstrained()) {
        // For constrained arrays the bound can be generated with reference to
        // the index subtype alone.
        IntegerType *indexTy = AE->getType();
        if (AE->isFirst())
            return emitScalarLowerBound(indexTy);
        else
            return emitScalarUpperBound(indexTy);
    }

    // Unconstrained arrays come in two flavours:  As actual parameters to a
    // subroutine or as the value of a function call.  We only cope with the
    // first possibility ATM.
    DeclRefExpr *ref;
    ParamValueDecl *param;

    ref = dyn_cast<DeclRefExpr>(AE->getPrefix());
    param = dyn_cast_or_null<ParamValueDecl>(ref->getDeclaration());
    if (!ref || !param) {
        assert(false && "Unconstrained array attribute not supported yet!");
        return 0;
    }

    llvm::Value *bounds = lookupBounds(param);
    unsigned offset = AE->getDimension() * 2;

    // The bounds structure is organized as a set of low/high pairs.  Offset
    // points to the low entry -- adjust if needed.
    if (AE->isLast())
        ++offset;

    // GEP and lod the required bound.
    llvm::Value *bound = Builder.CreateStructGEP(bounds, offset);
    return Builder.CreateLoad(bound);
}
