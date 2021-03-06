//===-- codegen/CodeGenAgg.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file defines the subroutines which handle code generation of
/// aggregate values.
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "comma/ast/AggExpr.h"
#include "comma/ast/Type.h"

#include <algorithm>

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

//===----------------------------------------------------------------------===//
/// \class
///
/// \brief Helper class for array aggregate emission.
class ArrayEmitter {

public:
    ArrayEmitter(CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder)
        : CGR(CGR),
          emitter(CGR),
          Builder(Builder) { }

    CValue emit(Expr *expr, llvm::Value *dst, bool genTmp);

    CValue emitAllocator(AllocatorExpr *expr);

private:
    CodeGenRoutine &CGR;
    BoundsEmitter emitter;
    llvm::IRBuilder<> &Builder;

    Frame *frame() { return CGR.getFrame(); }

    /// \brief Fills in the components defined by the given aggregates others
    /// clause (if any).
    ///
    /// \param agg The keyed aggregate to generate code for.  If \p agg does not
    /// define an others clause this emitter does nothing.
    ///
    /// \param dst Pointer to storage sufficient to hold the contents of this
    /// aggregate.
    ///
    /// \param lower Lower bound of the current index.
    ///
    /// \param upper Upper bound of the current index.
    void fillInOthers(AggregateExpr *agg, llvm::Value *dst,
                      llvm::Value *lower, llvm::Value *upper);

    /// \brief Emits the given expression repeatedly into a destination.
    ///
    /// \param others The expression to emit.  The given expression is evaluated
    /// repeatedly for each generated component.
    ///
    /// \param dst Pointer to storage sufficient to hold the generated data.
    ///
    /// \param start Index to start emitting \p others into.
    ///
    /// \param end One-past-the-last sentinel value.
    ///
    /// \param bias Value to subtract from \p start and \p end so that they are
    /// zero-based indices.
    void emitOthers(Expr *others, llvm::Value *dst,
                    llvm::Value *start, llvm::Value *end,
                    llvm::Value *bias);

    void emitOthers(AggregateExpr *expr, llvm::Value *dst,
                    llvm::Value *bounds, uint64_t numComponents);

    CValue emitPositionalAgg(AggregateExpr *expr, llvm::Value *dst);
    CValue emitKeyedAgg(AggregateExpr *expr, llvm::Value *dst);
    CValue emitStaticKeyedAgg(AggregateExpr *expr, llvm::Value *dst);
    CValue emitDynamicKeyedAgg(AggregateExpr *expr, llvm::Value *dst);
    CValue emitRangeKeyedAgg(AggregateExpr *expr, llvm::Value *dst);
    CValue emitOthersKeyedAgg(AggregateExpr *expr, llvm::Value *dst);

    CValue emitArrayConversion(ConversionExpr *convert, llvm::Value *dst,
                               bool genTmp);
    CValue emitCall(FunctionCallExpr *call, llvm::Value *dst);
    CValue emitAggregate(AggregateExpr *expr, llvm::Value *dst);
    CValue emitDefault(ArrayType *type, llvm::Value *dst);
    CValue emitStringLiteral(StringLiteral *expr);

    /// Allocates a temporary array.
    ///
    /// \param arrTy The Comma type of the array to allocate.
    ///
    /// \param bounds Precomputed bounds of the array type.
    ///
    /// \param dst Out parameter set to a pointer to the allocated array.
    ///
    /// \return If the allocation required the length of the array to be
    /// computed, returns that value, else null.
    llvm::Value *allocArray(ArrayType *arrTy, llvm::Value *bounds,
                            llvm::Value *&dst);

    /// Emits the components defined by a discrete choice.
    ///
    /// \param I Iterator yielding the component to emit.
    ///
    /// \param dst Pointer to storage sufficient to hold the aggregate data.
    ///
    /// \param bias The lower bound of the associated discrete index type.  This
    /// value is used to correct the actual index values defined by the discrete
    /// choice so that they are zero based.
    void emitDiscreteComponent(AggregateExpr::key_iterator &I,
                               llvm::Value *dst, llvm::Value *bias);

    /// \brief Emits the array component given by \p expr and stores the result
    /// in \p dst.
    void emitComponent(Expr *expr, llvm::Value *dst);

    /// Converts the given value to an unsigned pointer sized integer if needed.
    llvm::Value *convertIndex(llvm::Value *idx);

    /// \name Allocator Methods.
    //@{

    /// Emits an allocation of a definite array type.
    CValue emitDefiniteAllocator(AllocatorExpr *expr, ArrayType *arrTy);

    /// Emits an allocation for a constrained array type, returning the result
    /// as a fat access value.
    CValue emitConstrainedAllocator(AllocatorExpr *expr, ArrayType *arrTy);

    /// Emits an allocation initialized by a function call.
    CValue emitCallAllocator(AllocatorExpr *expr,
                             FunctionCallExpr *call, ArrayType *arrTy);

    /// Emits an allocation initialized by an unconstrained value.
    CValue emitValueAllocator(AllocatorExpr *expr,
                              ValueDecl *value, ArrayType *arrTy);
    //@}
};

llvm::Value *ArrayEmitter::convertIndex(llvm::Value *idx)
{
    const llvm::Type *intptrTy = CGR.getCodeGen().getIntPtrTy();
    if (idx->getType() != intptrTy)
        return Builder.CreateIntCast(idx, intptrTy, false);
    return idx;
}

CValue ArrayEmitter::emitAllocator(AllocatorExpr *expr)
{
    ArrayType *arrTy;
    arrTy = cast<ArrayType>(CGR.resolveType(expr->getAllocatedType()));

    if (arrTy->isDefiniteType())
        return emitDefiniteAllocator(expr, arrTy);

    if (arrTy->isConstrained())
        return emitConstrainedAllocator(expr, arrTy);

    assert(expr->isInitialized() &&
           "Cannot codegen indefinite unconstrained aggregate allocators!");

    // Resolve the initialization expression and corresponding type.
    Expr *init = expr->getInitializer();
    while (QualifiedExpr *qual = dyn_cast<QualifiedExpr>(init))
        init = qual->getOperand();
    arrTy = cast<ArrayType>(init->getType());

    // Treat the allocator as constrained if its initializer is.
    if (arrTy->isConstrained())
        return emitConstrainedAllocator(expr, arrTy);

    // Function calls, formal parameters and object declarations are the only
    // types of expression which can be of unconstrained array type.
    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(init))
        return emitCallAllocator(expr, call, arrTy);

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(init)) {
        ValueDecl *value = dyn_cast<ValueDecl>(ref->getDeclaration());
        if (value)
            return emitValueAllocator(expr, value, arrTy);
    }

    assert(false && "Unexpected unconstrained allocator initializer!");
    return CValue::getFat(0);
}

CValue ArrayEmitter::emitDefiniteAllocator(AllocatorExpr *expr,
                                           ArrayType *arrTy)
{
    assert(arrTy->isDefiniteType());

    const llvm::ArrayType *loweredTy;
    const llvm::PointerType *resultTy;
    CodeGenTypes &CGT = CGR.getCodeGen().getCGT();
    CommaRT &CRT = CGR.getCodeGen().getRuntime();

    loweredTy = CGT.lowerArrayType(arrTy);
    resultTy = CGT.lowerThinAccessType(expr->getType());

    uint64_t size = CGT.getTypeSize(loweredTy);
    unsigned align = CGT.getTypeAlignment(loweredTy);

    llvm::Value *result = CRT.comma_alloc(Builder, size, align);
    result = Builder.CreatePointerCast(result, resultTy);

    if (expr->isInitialized())
        emit(expr->getInitializer(), result, false);

    return CValue::get(result);
}

CValue ArrayEmitter::emitConstrainedAllocator(AllocatorExpr *expr,
                                              ArrayType *arrTy)
{
    assert(arrTy->isConstrained());

    const llvm::ArrayType *loweredTy;
    const llvm::StructType *resultTy;
    const llvm::PointerType *dataTy;
    CodeGen &CG = CGR.getCodeGen();
    CodeGenTypes &CGT = CG.getCGT();
    CommaRT &CRT = CG.getRuntime();

    loweredTy = CGT.lowerArrayType(arrTy);
    resultTy = CGT.lowerFatAccessType(expr->getType());
    dataTy = cast<llvm::PointerType>(resultTy->getElementType(0));

    // Compute the size and bounds of the array.  Use static information if
    // possible, otherwise compute at runtime.
    BoundsEmitter emitter(CGR);
    llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);
    llvm::Value *size;
    if (arrTy->isStaticallyConstrained()) {
        uint64_t staticSize = CGT.getTypeSize(loweredTy);
        size = llvm::ConstantInt::get(CG.getInt32Ty(), staticSize);
    }
    else {
        uint64_t staticSize = CGT.getTypeSize(loweredTy->getElementType());
        llvm::Value *length = emitter.computeTotalBoundLength(Builder, bounds);
        size = llvm::ConstantInt::get(CG.getInt32Ty(), staticSize);
        size = Builder.CreateMul(size, length);
    }

    // Allocate space for the data and cast the raw memory pointer to the
    // expected type.  Evaluate the initializer using the allocated memory as
    // storage.
    unsigned align = CGT.getTypeAlignment(loweredTy);
    llvm::Value *data = CRT.comma_alloc(Builder, size, align);
    data = Builder.CreatePointerCast(data, dataTy);

    // Create a temporary to hold the fat pointer and store the data pointer and
    // bounds.
    llvm::Value *fatPtr = frame()->createTemp(resultTy);
    if (isa<llvm::PointerType>(bounds->getType()))
        bounds = Builder.CreateLoad(bounds);
    Builder.CreateStore(bounds, Builder.CreateStructGEP(fatPtr, 1));
    Builder.CreateStore(data, Builder.CreateStructGEP(fatPtr, 0));

    // Initialize the data if needed.
    //
    // FIXME: Perform default initialization otherwise.
    if (expr->isInitialized())
        emit(expr->getInitializer(), data, false);

    // Finished.
    return CValue::getFat(fatPtr);
}

CValue ArrayEmitter::emitCallAllocator(AllocatorExpr *expr,
                                       FunctionCallExpr *call,
                                       ArrayType *arrTy)
{
    CodeGen &CG = CGR.getCodeGen();
    CommaRT &CRT = CG.getRuntime();
    CodeGenTypes &CGT = CG.getCGT();

    const llvm::Type *componentTy;
    const llvm::StructType *boundsTy;
    const llvm::PointerType *targetTy;
    const llvm::StructType *fatTy;

    componentTy = CGT.lowerType(arrTy->getComponentType());
    boundsTy = CGT.lowerArrayBounds(arrTy);
    targetTy = CG.getVLArrayTy(componentTy)->getPointerTo();
    fatTy = CGT.lowerFatAccessType(expr->getType());

    llvm::Value *fatPtr = frame()->createTemp(fatTy);

    // Emit a "simple" call, thereby leaving the bounds and data on the vstack.
    CGR.emitSimpleCall(call);

    // Get a reference to the bounds and compute the size of the needed array.
    llvm::Value *bounds = CRT.vstack(Builder, boundsTy->getPointerTo());
    llvm::Value *length = emitter.computeTotalBoundLength(Builder, bounds);

    uint64_t size = CGT.getTypeSize(componentTy);
    unsigned align = CGT.getTypeAlignment(componentTy);

    llvm::Value *dimension = Builder.CreateMul
        (length, llvm::ConstantInt::get(length->getType(), size));

    // Store the bounds into the fat pointer structure.
    Builder.CreateStore(Builder.CreateLoad(bounds),
                        Builder.CreateStructGEP(fatPtr, 1));

    // Allocate a destination for the array.
    llvm::Value *dst = CRT.comma_alloc(Builder, dimension, align);

    // Pop the vstack and obtain a pointer to the component data.
    CRT.vstack_pop(Builder);
    llvm::Value *data = CRT.vstack(Builder, CG.getInt8PtrTy());

    // Store the data into the destination.
    llvm::Function *memcpy = CG.getMemcpy32();
    Builder.CreateCall4(memcpy, dst, data, dimension,
                        llvm::ConstantInt::get(CG.getInt32Ty(), align));

    // Update the fat pointer with the allocated data.
    Builder.CreateStore(Builder.CreatePointerCast(dst, targetTy),
                        Builder.CreateStructGEP(fatPtr, 0));

    // Pop the component data from the vstack.
    CRT.vstack_pop(Builder);

    return CValue::getFat(fatPtr);
}

CValue ArrayEmitter::emitValueAllocator(AllocatorExpr *expr,
                                        ValueDecl *param,
                                        ArrayType *arrTy)
{
    CodeGen &CG = CGR.getCodeGen();
    CommaRT &CRT = CG.getRuntime();
    CodeGenTypes &CGT = CG.getCGT();

    const llvm::Type *componentTy;
    const llvm::PointerType *targetTy;
    const llvm::StructType *fatTy;

    componentTy = CGT.lowerType(arrTy->getComponentType());
    targetTy = CG.getVLArrayTy(componentTy)->getPointerTo();
    fatTy = CGT.lowerFatAccessType(expr->getType());

    llvm::Value *fatPtr = frame()->createTemp(fatTy);
    llvm::Value *data   = frame()->lookup(param, activation::Slot);
    llvm::Value *bounds = frame()->lookup(param, activation::Bounds);
    llvm::Value *length = emitter.computeTotalBoundLength(Builder, bounds);

    // Allocate the required memory and copy the data over.
    uint64_t size = CGT.getTypeSize(componentTy);
    unsigned align = CGT.getTypeAlignment(componentTy);

    llvm::Value *dimension = Builder.CreateMul
        (length, llvm::ConstantInt::get(length->getType(), size));

    // Store the bounds into the fat pointer structure.
    Builder.CreateStore(Builder.CreateLoad(bounds),
                        Builder.CreateStructGEP(fatPtr, 1));

    // Allocate a destination for the array and stor the data into the
    // destination.
    llvm::Value *dst = CRT.comma_alloc(Builder, dimension, align);
    llvm::Function *memcpy = CG.getMemcpy32();
    data = Builder.CreatePointerCast(data, CG.getInt8PtrTy());
    Builder.CreateCall4(memcpy, dst, data, dimension,
                        llvm::ConstantInt::get(CG.getInt32Ty(), align));

    // Update the fat pointer with the allocated data.
    Builder.CreateStore(Builder.CreatePointerCast(dst, targetTy),
                        Builder.CreateStructGEP(fatPtr, 0));

    return CValue::getFat(fatPtr);
}


CValue ArrayEmitter::emit(Expr *expr, llvm::Value *dst, bool genTmp)
{
    llvm::Value *components;
    llvm::Value *bounds;
    ArrayType *arrTy = cast<ArrayType>(CGR.resolveType(expr->getType()));

    if (ConversionExpr *convert = dyn_cast<ConversionExpr>(expr))
        return emitArrayConversion(convert, dst, genTmp);

    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(expr))
        return emitCall(call, dst);

    if (AggregateExpr *agg = dyn_cast<AggregateExpr>(expr))
        return emitAggregate(agg, dst);

    if (QualifiedExpr *qual = dyn_cast<QualifiedExpr>(expr))
        return emit(qual->getOperand(), dst, genTmp);

    if (isa<DiamondExpr>(expr))
        return emitDefault(arrTy, dst);

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        components = frame()->lookup(decl, activation::Slot);
        if (!(bounds = frame()->lookup(decl, activation::Bounds)))
            bounds = emitter.synthStaticArrayBounds(Builder, arrTy);
    }
    else if (StringLiteral *lit = dyn_cast<StringLiteral>(expr)) {
        CValue value = emitStringLiteral(lit);
        components = value.first();
        bounds = value.second();
    }
    else if (IndexedArrayExpr *iae = dyn_cast<IndexedArrayExpr>(expr)) {
        CValue value = CGR.emitIndexedArrayRef(iae);
        components = value.first();
        bounds = value.second();
    }
    else if (SelectedExpr *sel = dyn_cast<SelectedExpr>(expr)) {
        components = CGR.emitSelectedRef(sel).first();
        bounds = emitter.synthArrayBounds(Builder, arrTy);
    }
    else if (DereferenceExpr *deref = dyn_cast<DereferenceExpr>(expr)) {
        CValue value = CGR.emitValue(deref->getPrefix());

        // If the dereference is with respect to a fat pointer then the first
        // component is the data pointer and the second is the bounds
        // structure.  Otherwise we have a simple thin pointer to an array of
        // staticly known size.
        if (value.isFat()) {
            llvm::Value *fatPtr = value.first();
            components = Builder.CreateStructGEP(fatPtr, 0);
            components = Builder.CreateLoad(components);
            bounds = Builder.CreateStructGEP(fatPtr, 1);
        }
        else {
            components = value.first();
            bounds = emitter.synthArrayBounds(Builder, arrTy);
        }

        // Check that the component pointer is non-null.
        CGR.emitNullAccessCheck(components, deref->getLocation());
    }
    else {
        assert(false && "Invalid type of array expr!");
        return CValue::getArray(0, 0);
    }

    llvm::Value *length = 0;

    // If dst is null build a stack allocated object to hold the components of
    // this array.
    if (dst == 0 && genTmp)
        length = allocArray(arrTy, bounds, dst);

    // If a destination is available, fill it in with the associated array
    // data.  Note that dst is of type [N x T]*, hense the double `dereference'
    // to get at the element type.
    if (dst) {
        const llvm::Type *componentTy = dst->getType();
        componentTy = cast<llvm::SequentialType>(componentTy)->getElementType();
        componentTy = cast<llvm::SequentialType>(componentTy)->getElementType();
        if (!length)
            length = emitter.computeTotalBoundLength(Builder, bounds);
        CGR.emitArrayCopy(components, dst, length, componentTy);
        return CValue::getArray(dst, bounds);
    }
    else
        return CValue::getArray(components, bounds);
}

void ArrayEmitter::emitComponent(Expr *expr, llvm::Value *dst)
{
    Type *exprTy = CGR.resolveType(expr);

    if (exprTy->isCompositeType())
        CGR.emitCompositeExpr(expr, dst, false);
    else if (exprTy->isFatAccessType()) {
        llvm::Value *fatPtr = CGR.emitValue(expr).first();
        Builder.CreateStore(Builder.CreateLoad(fatPtr), dst);
    }
    else
        Builder.CreateStore(CGR.emitValue(expr).first(), dst);
}

CValue ArrayEmitter::emitCall(FunctionCallExpr *call, llvm::Value *dst)
{
    ArrayType *arrTy = cast<ArrayType>(CGR.resolveType(call->getType()));

    // Constrained types use the sret call convention.
    if (arrTy->isConstrained()) {
        CValue data = CGR.emitCompositeCall(call, dst);
        llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);
        return CValue::getArray(data.first(), bounds);
    }

    // Unconstrained (indefinite type) use the vstack.  The callee cannot know
    // the size of the result hense dst must be null.
    assert(dst == 0 && "Destination given for indefinite type!");
    return CGR.emitVStackCall(call);
}

CValue ArrayEmitter::emitAggregate(AggregateExpr *expr, llvm::Value *dst)
{
    if (expr->isPurelyPositional())
        return emitPositionalAgg(expr, dst);
    return emitKeyedAgg(expr, dst);
}

CValue ArrayEmitter::emitDefault(ArrayType *arrTy, llvm::Value *dst)
{
    CodeGen &CG = CGR.getCodeGen();
    CodeGenTypes &CGT = CG.getCGT();

    llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);
    llvm::Value *length = 0;

    // If a destination is not provided allocate a temporary of the required size.
    if (!dst)
        length = allocArray(arrTy, bounds, dst);
    else
        length = emitter.computeTotalBoundLength(Builder, bounds);

    // Memset the destination to zero.
    const llvm::Type *componentTy = CGT.lowerType(arrTy->getComponentType());
    uint64_t componentSize = CGT.getTypeSize(componentTy);
    unsigned align = CGT.getTypeAlignment(componentTy);
    llvm::Function *memset = CG.getMemset32();
    llvm::Value *size = Builder.CreateMul
        (length, llvm::ConstantInt::get(length->getType(), componentSize));
    llvm::Value *raw = Builder.CreatePointerCast(dst, CG.getInt8PtrTy());

    Builder.CreateCall4(memset, raw, llvm::ConstantInt::get(CG.getInt8Ty(), 0),
                        size, llvm::ConstantInt::get(CG.getInt32Ty(), align));

    // Return the generated aggregate.
    return CValue::getArray(dst, bounds);
}

CValue ArrayEmitter::emitStringLiteral(StringLiteral *expr)
{
    assert(expr->hasType() && "Unresolved string literal type!");

    // Build a constant array representing the literal.
    CodeGen &CG = CGR.getCodeGen();
    CodeGenTypes &CGT = CG.getCGT();
    const llvm::ArrayType *arrTy = CGT.lowerArrayType(expr->getType());
    const llvm::Type *elemTy = arrTy->getElementType();
    llvm::StringRef string = expr->getString();

    std::vector<llvm::Constant *> elements;
    const EnumerationDecl *component = expr->getComponentType();
    for (llvm::StringRef::iterator I = string.begin(); I != string.end(); ++I) {
        unsigned encoding = component->getEncoding(*I);
        elements.push_back(llvm::ConstantInt::get(elemTy, encoding));
    }

    // Build a constant global array to represent this literal.
    llvm::Constant *arrData;
    llvm::GlobalVariable *dataPtr;
    arrData = CG.getConstantArray(elemTy, elements);
    dataPtr = new llvm::GlobalVariable(
        *CG.getModule(), arrData->getType(), true,
        llvm::GlobalValue::InternalLinkage, arrData, "string.data");

    // Likewise, build a constant global to represent the bounds.  Emitting the
    // bounds into memory should be slightly faster then building them on the
    // stack in the majority of cases.
    llvm::Constant *boundData;
    llvm::GlobalVariable *boundPtr;
    boundData = emitter.synthStaticArrayBounds(Builder, expr->getType());
    boundPtr = new llvm::GlobalVariable(
        *CG.getModule(), boundData->getType(), true,
        llvm::GlobalValue::InternalLinkage, boundData, "bounds.data");
    return CValue::getArray(dataPtr, boundPtr);
}

CValue ArrayEmitter::emitArrayConversion(ConversionExpr *convert,
                                         llvm::Value *dst, bool genTmp)
{
    // FIXME: Implement.
    return emit(convert->getOperand(), dst, genTmp);
}

void ArrayEmitter::emitOthers(Expr *others, llvm::Value *dst,
                              llvm::Value *start, llvm::Value *end,
                              llvm::Value *bias)
{
    llvm::BasicBlock *startBB = Builder.GetInsertBlock();
    llvm::BasicBlock *checkBB = frame()->makeBasicBlock("others.check");
    llvm::BasicBlock *bodyBB = frame()->makeBasicBlock("others.body");
    llvm::BasicBlock *mergeBB = frame()->makeBasicBlock("others.merge");

    // Initialize the iteration and sentinal values.
    llvm::Value *iterStart = Builder.CreateSub(start, bias);
    llvm::Value *iterLimit = Builder.CreateSub(end, bias);

    const llvm::Type *iterTy = iterStart->getType();
    llvm::Value *iterZero = llvm::ConstantInt::get(iterTy, 0);
    llvm::Value *iterOne = llvm::ConstantInt::get(iterTy, 1);
    Builder.CreateBr(checkBB);

    // Loop header.
    Builder.SetInsertPoint(checkBB);
    llvm::PHINode *phi = Builder.CreatePHI(iterStart->getType());
    llvm::Value *pred = Builder.CreateICmpEQ(phi, iterLimit);
    Builder.CreateCondBr(pred, mergeBB, bodyBB);

    // Loop body.
    Builder.SetInsertPoint(bodyBB);
    llvm::Value *indices[2];
    indices[0] = iterZero;
    indices[1] = convertIndex(phi);

    llvm::Value *ptr = Builder.CreateInBoundsGEP(dst, indices, indices + 2);
    emitComponent(others, ptr);
    llvm::Value *iterNext = Builder.CreateAdd(phi, iterOne);
    Builder.CreateBr(checkBB);

    // Populate the phi node with the incoming values.
    phi->addIncoming(iterStart, startBB);
    phi->addIncoming(iterNext, Builder.GetInsertBlock());

    // Switch to merge.
    Builder.SetInsertPoint(mergeBB);
}

void ArrayEmitter::emitOthers(AggregateExpr *expr,
                              llvm::Value *dst, llvm::Value *bounds,
                              uint64_t numComponents)
{
    // If this aggregate does not specify an others component we are done.
    //
    // FIXME: If an undefined others component is specified we should be
    // initializing the components with their default value (if any).
    Expr *othersExpr = expr->getOthersExpr();
    if (!othersExpr)
        return;

    // If there were no positional components emitted, emit a single "others"
    // expression.  This simplifies emission of the remaining elements since we
    // can "count from zero" and compare against the upper bound without
    // worrying about overflow.
    if (numComponents == 0) {
        emitComponent(othersExpr, Builder.CreateConstGEP2_32(dst, 0, 0));
        numComponents = 1;
    }

    // Synthesize a loop to populate the remaining components.  Note that a
    // memset is not possible here since we must re-evaluate the associated
    // expression for each component.
    llvm::BasicBlock *startBB = Builder.GetInsertBlock();
    llvm::BasicBlock *checkBB = frame()->makeBasicBlock("others.check");
    llvm::BasicBlock *bodyBB = frame()->makeBasicBlock("others.body");
    llvm::BasicBlock *mergeBB = frame()->makeBasicBlock("others.merge");

    // Compute the number of "other" components minus one that we need to emit.
    llvm::Value *lower = BoundsEmitter::getLowerBound(Builder, bounds, 0);
    llvm::Value *upper = BoundsEmitter::getUpperBound(Builder, bounds, 0);
    llvm::Value *max = Builder.CreateSub(upper, lower);

    // Initialize the iteration variable to the number of components we have
    // already emitted minus one (the subtraction is always valid since we
    // guaranteed above that at least one component has been generated).
    const llvm::IntegerType *idxTy = cast<llvm::IntegerType>(upper->getType());
    llvm::Value *idxZero = llvm::ConstantInt::get(idxTy, 0);
    llvm::Value *idxOne = llvm::ConstantInt::get(idxTy, 1);
    llvm::Value *idxStart = llvm::ConstantInt::get(idxTy, numComponents - 1);

    // Branch to the check BB and test if the index is equal to max.  If it is
    // we are done.
    Builder.CreateBr(checkBB);
    Builder.SetInsertPoint(checkBB);
    llvm::PHINode *phi = Builder.CreatePHI(idxTy);
    Builder.CreateCondBr(Builder.CreateICmpEQ(phi, max), mergeBB, bodyBB);

    // Move to the body block.  Increment our index and emit the component.
    Builder.SetInsertPoint(bodyBB);
    llvm::Value *idxNext = Builder.CreateAdd(phi, idxOne);

    llvm::Value *indices[2];
    indices[0] = idxZero;
    indices[1] = convertIndex(idxNext);
    llvm::Value *ptr = Builder.CreateInBoundsGEP(dst, indices, indices + 2);
    emitComponent(othersExpr, ptr);
    Builder.CreateBr(checkBB);

    // Add the incoming values to our phi node.
    phi->addIncoming(idxStart, startBB);
    phi->addIncoming(idxNext, Builder.GetInsertBlock());

    // Set the insert point to the merge BB and return.
    Builder.SetInsertPoint(mergeBB);
}

CValue ArrayEmitter::emitPositionalAgg(AggregateExpr *expr, llvm::Value *dst)
{
    assert(expr->isPurelyPositional() && "Unexpected type of aggregate!");

    llvm::Value *bounds = emitter.synthAggregateBounds(Builder, expr);
    ArrayType *arrTy = cast<ArrayType>(expr->getType());

    std::vector<llvm::Value*> components;

    if (dst == 0)
        allocArray(arrTy, bounds, dst);

    typedef AggregateExpr::pos_iterator iterator;
    iterator I = expr->pos_begin();
    iterator E = expr->pos_end();
    for (unsigned idx = 0; I != E; ++I, ++idx)
        emitComponent(*I, Builder.CreateConstGEP2_32(dst, 0, idx));

    // Emit an "others" clause if present.
    emitOthers(expr, dst, bounds, components.size());
    return CValue::getArray(dst, bounds);
}

CValue ArrayEmitter::emitKeyedAgg(AggregateExpr *expr, llvm::Value *dst)
{
    assert(expr->isPurelyKeyed() && "Unexpected kind of aggregate!");

    if (expr->hasStaticIndices())
        return emitStaticKeyedAgg(expr, dst);

    if (expr->numKeys() == 1)
        return emitDynamicKeyedAgg(expr, dst);

    assert(expr->numKeys() == 0);
    assert(expr->hasOthers());

    return emitOthersKeyedAgg(expr, dst);
}

void ArrayEmitter::emitDiscreteComponent(AggregateExpr::key_iterator &I,
                                         llvm::Value *dst, llvm::Value *bias)
{
    // Number of components we need to emit.
    uint64_t length;

    // Starting index to emit each component (not corrected by the given bias).
    llvm::Value *idx;

    if (Range *range = (*I)->getAsRange()) {
        length = range->length();
        idx = emitter.getLowerBound(Builder, range);
    }
    else {
        Expr *expr = (*I)->getAsExpr();
        length = 1;
        idx = CGR.emitValue(expr).first();
    }

    Expr *expr = I.getExpr();
    const llvm::Type *idxTy = idx->getType();

    // Perform the emission using inline code if length is small.  Otherwise use
    // a loop.
    //
    // FIXME: The value 8 here was chosen arbitrarily and very likely not ideal.
    // In particular, LLVM does a good job of transforming sequential code like
    // this into a memset when the expression is constant.
    if (length <= 8) {
        llvm::Value *idxZero = llvm::ConstantInt::get(idxTy, 0);
        llvm::Value *idxOne = llvm::ConstantInt::get(idxTy, 1);
        idx = Builder.CreateSub(idx, bias);
        for (uint64_t i = 0; i < length; ++i) {
            llvm::Value *indices[2];
            llvm::Value *ptr;
            indices[0] = idxZero;
            indices[1] = convertIndex(idx);
            ptr = Builder.CreateInBoundsGEP(dst, indices, indices + 2);
            emitComponent(expr, ptr);
            idx = Builder.CreateAdd(idx, idxOne);
        }
    }
    else {
        llvm::Value *end = llvm::ConstantInt::get(idxTy, length);
        end = Builder.CreateAdd(idx, end);
        emitOthers(expr, dst, idx, end, bias);
    }
}

CValue ArrayEmitter::emitStaticKeyedAgg(AggregateExpr *agg, llvm::Value *dst)
{
    assert(agg->isPurelyKeyed() && "Unexpected type of aggregate!");

    ArrayType *arrTy = cast<ArrayType>(agg->getType());
    DiscreteType *idxTy = arrTy->getIndexType(0);

    // Build a bounds structure for this aggregate.
    llvm::Value *bounds;
    llvm::Value *lower;
    llvm::Value *upper;

    bounds = emitter.synthScalarBounds(Builder, idxTy);
    lower = emitter.getLowerBound(Builder, bounds, 0);
    upper = emitter.getUpperBound(Builder, bounds, 0);

    // If the destination is null, create a temporary to hold the aggregate.
    if (dst == 0)
        allocArray(arrTy, bounds, dst);

    // Generate the aggregate.
    AggregateExpr::key_iterator I = agg->key_begin();
    AggregateExpr::key_iterator E = agg->key_end();
    for ( ; I != E; ++I)
        emitDiscreteComponent(I, dst, lower);
    fillInOthers(agg, dst, lower, upper);

    return CValue::getArray(dst, bounds);
}

void ArrayEmitter::fillInOthers(AggregateExpr *agg, llvm::Value *dst,
                                llvm::Value *lower, llvm::Value *upper)
{
    // If there is no others expression we are done.
    Expr *others = agg->getOthersExpr();
    if (!others)
        return;

    DiscreteType *idxTy = cast<ArrayType>(agg->getType())->getIndexType(0);
    const llvm::Type *iterTy = lower->getType();

    // Build a sorted vector of the keys supplied by the aggregate.
    typedef std::vector<ComponentKey*> KeyVec;
    KeyVec KV(agg->key_begin(), agg->key_end());
    if (idxTy->isSigned())
        std::sort(KV.begin(), KV.end(), ComponentKey::compareKeysS);
    else
        std::sort(KV.begin(), KV.end(), ComponentKey::compareKeysU);

    llvm::APInt limit;
    llvm::APInt lowerValue;
    llvm::APInt upperValue;

    // Fill in any missing leading elements.
    KV.front()->getLowerValue(lowerValue);
    idxTy->getLowerLimit(limit);
    if (lowerValue != limit) {
        llvm::Value *end = llvm::ConstantInt::get(iterTy, lowerValue);
        emitOthers(others, dst, lower, end, lower);
    }

    // Fill in each interior "hole".
    for (unsigned i = 0; i < KV.size() - 1; ++i) {
        // Note the change in the sense of "upper" and "lower" here.
        KV[i]->getUpperValue(lowerValue);
        KV[i+1]->getLowerValue(upperValue);

        if (lowerValue == upperValue)
            continue;

        llvm::Value *start = llvm::ConstantInt::get(iterTy, ++lowerValue);
        llvm::Value *end = llvm::ConstantInt::get(iterTy, upperValue);
        emitOthers(others, dst, start, end, lower);
    }

    // Fill in any missing trailing elements.
    KV.back()->getUpperValue(upperValue);
    idxTy->getUpperLimit(limit);
    if (upperValue != limit) {
        llvm::Value *start;
        llvm::Value *end;
        start = llvm::ConstantInt::get(iterTy, upperValue);
        start = Builder.CreateAdd(start, llvm::ConstantInt::get(iterTy, 1));
        end = Builder.CreateAdd(upper, llvm::ConstantInt::get(iterTy, 1));
        emitOthers(others, dst, start, end, lower);
    }
}

CValue ArrayEmitter::emitRangeKeyedAgg(AggregateExpr *expr, llvm::Value *dst)
{
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    AggregateExpr::key_iterator I = expr->key_begin();
    Range *range = cast<Range>((*I)->getRep());
    llvm::Value *bounds = emitter.synthRange(Builder, range);
    llvm::Value *length = 0;

    if (dst == 0)
        length = allocArray(arrTy, bounds, dst);

    if (length == 0)
        length = emitter.computeBoundLength(Builder, bounds, 0);

    // Iterate from 0 upto the computed length of the aggregate.  Define the
    // iteration type and some constance for convenience.
    const llvm::Type *iterTy = length->getType();
    llvm::Value *iterZero = llvm::ConstantInt::get(iterTy, 0);
    llvm::Value *iterOne = llvm::ConstantInt::get(iterTy, 1);

    // Define the iteration variable as an alloca.
    llvm::Value *iter = frame()->createTemp(iterTy);
    Builder.CreateStore(iterZero, iter);

    // Create check, body, and merge basic blocks.
    llvm::BasicBlock *checkBB = frame()->makeBasicBlock("agg.check");
    llvm::BasicBlock *bodyBB = frame()->makeBasicBlock("agg.body");
    llvm::BasicBlock *mergeBB = frame()->makeBasicBlock("agg.merge");

    // While iter < length.
    Builder.CreateBr(checkBB);
    Builder.SetInsertPoint(checkBB);
    llvm::Value *idx = Builder.CreateLoad(iter);
    llvm::Value *pred = Builder.CreateICmpULT(idx, length);
    Builder.CreateCondBr(pred, bodyBB, mergeBB);

    // Compute and store an element into the aggregate.
    Builder.SetInsertPoint(bodyBB);
    llvm::Value *indices[2];
    llvm::Value *ptr;

    indices[0] = iterZero;
    indices[1] = convertIndex(idx);

    ptr = Builder.CreateInBoundsGEP(dst, indices, indices + 2);
    emitComponent(I.getExpr(), ptr);
    Builder.CreateStore(Builder.CreateAdd(idx, iterOne), iter);
    Builder.CreateBr(checkBB);

    // Set the insert point to the merge block and return.
    Builder.SetInsertPoint(mergeBB);
    return CValue::getArray(dst, bounds);
}

CValue ArrayEmitter::emitDynamicKeyedAgg(AggregateExpr *expr, llvm::Value *dst)
{
    AggregateExpr::key_iterator I = expr->key_begin();

    // The key is either a range or a simple expression.
    if (I->getAsRange())
        return emitRangeKeyedAgg(expr, dst);

    // The case of a simple expression is trivial.  Allocate an array with one
    // element and store the result.  We evaluate the index regardless to ensure
    // any checks are performed.  This type of aggregate is very similar to an
    // assignment.
    Expr *key = I->getAsExpr();
    Expr *value = I.getExpr();
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);

    if (dst == 0)
        allocArray(arrTy, bounds, dst);

    CGR.emitValue(key);
    llvm::Value *ptr = Builder.CreateConstInBoundsGEP2_64(dst, 0, 0);
    emitComponent(value, ptr);
    return CValue::getArray(dst, bounds);
}

CValue ArrayEmitter::emitOthersKeyedAgg(AggregateExpr *expr, llvm::Value *dst)
{
    assert(expr->hasOthers() && "Empty aggregate!");

    // For an others expression, obtain bounds from the index type of
    // this aggregate.
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    assert(arrTy->isConstrained() && "Aggregate requires constraint!");

    DiscreteType *idxTy = arrTy->getIndexType(0);
    llvm::Value *bounds = emitter.synthScalarBounds(Builder, idxTy);

    // Allocate a destination if needed.
    if (dst == 0)
        allocArray(arrTy, bounds, dst);

    // Emit the others expression.
    emitOthers(expr, dst, bounds, 0);
    return CValue::getArray(dst, bounds);
}

llvm::Value *ArrayEmitter::allocArray(ArrayType *arrTy, llvm::Value *bounds,
                                      llvm::Value *&dst)
{
    CodeGen &CG = CGR.getCodeGen();
    CodeGenTypes &CGT = CG.getCGT();

    if (arrTy->isStaticallyConstrained()) {
        dst = frame()->createTemp(CGT.lowerArrayType(arrTy));
        return 0;
    }
    else {
        const llvm::Type *componentTy;
        const llvm::Type *dstTy;
        llvm::Value *length;

        length = emitter.computeTotalBoundLength(Builder, bounds);
        componentTy = CGT.lowerType(arrTy->getComponentType());
        dstTy = CG.getPointerType(CG.getVLArrayTy(componentTy));
        frame()->stacksave();
        dst = Builder.CreateAlloca(componentTy, length);
        dst = Builder.CreatePointerCast(dst, dstTy);
        return length;
    }
}

//===----------------------------------------------------------------------===//
/// \class
///
/// \brief Helper class for record aggregate emission.
class RecordEmitter {

public:
    RecordEmitter(CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder)
        : CGR(CGR),
          CGT(CGR.getCodeGen().getCGT()),
          emitter(CGR),
          Builder(Builder) { }

    CValue emit(Expr *expr, llvm::Value *dst, bool genTmp);

    CValue emitAllocator(AllocatorExpr *expr);

private:
    CodeGenRoutine &CGR;
    CodeGenTypes &CGT;
    BoundsEmitter emitter;
    llvm::IRBuilder<> &Builder;

    Frame *frame() { return CGR.getFrame(); }

    /// Emits a function call expression returning a record type as result.
    CValue emitCall(FunctionCallExpr *call, llvm::Value *dst);

    /// Emits an aggregate expression of record type.
    ///
    /// \param agg The aggregate to emit.
    ///
    /// \param dst A pointer to a corresponding llvm structure to hold the
    /// result of the aggregate or null.  When \p dst is null a temporary will
    /// be allocated to hold the aggregate.
    ///
    /// \return A CValue consisting of a pointer to an llvm structure populated
    /// with the components defined by \p agg.
    CValue emitAggregate(AggregateExpr *agg, llvm::Value *dst);

    /// Emits a default initialized value of the given record type.
    CValue emitDefault(RecordType *type, llvm::Value *dst);

    /// A reference to the following type is passed to the various helper
    /// methods which implement emitAggregate.  Each method populates the set
    /// with the components they emitted.  This set is used to implement
    /// emitOthersComponents.
    typedef llvm::SmallPtrSet<ComponentDecl*, 16> ComponentSet;

    /// Helper for emitAggregate.
    ///
    /// Populates \p dst with any positional components provided by \p agg.
    /// This method is a noop if \p agg does not admit any positional
    /// components.  Populates \p components with all ComponentDecl's emitted.
    void emitPositionalComponents(AggregateExpr *agg, llvm::Value *dst,
                                  ComponentSet &components);

    /// Helper for emitAggregate.
    ///
    /// Populates \p dst with any keyed components provided by \p agg.  This
    /// method is a noop if \p agg does not admit any keyed components.
    /// Populates \p components with all ComponentDecl's emitted.
    void emitKeyedComponents(AggregateExpr *agg, llvm::Value *dst,
                             ComponentSet &components);

    /// Helper for emitAggregate.
    ///
    /// Populates \p dst with all components defined by the type of \p agg but
    /// which do not appear in \p components.  This method is a noop if \p agg
    /// does not define an others clause.
    void emitOthersComponents(AggregateExpr *agg, llvm::Value *dst,
                              ComponentSet &components);

    /// Helper for emitAggregate.
    ///
    /// Emits the given expression into the location pointed to by \p dst.
    void emitComponent(Expr *expr, llvm::Value *dst);
};

CValue RecordEmitter::emitAllocator(AllocatorExpr *expr)
{
    // Compute the size and alignment of the record to be allocated.
    AccessType *exprTy = expr->getType();
    const llvm::PointerType *resultTy = CGT.lowerThinAccessType(exprTy);
    const llvm::Type *pointeeTy = resultTy->getElementType();

    uint64_t size = CGT.getTypeSize(pointeeTy);
    unsigned align = CGT.getTypeAlignment(pointeeTy);

    // Call into the runtime to allocate the object and cast the result to the
    // needed type.
    CommaRT &CRT = CGR.getCodeGen().getRuntime();
    llvm::Value *pointer = CRT.comma_alloc(Builder, size, align);
    pointer = Builder.CreatePointerCast(pointer, resultTy);

    // If an initializer is given emit the record into the allocated memory.
    if (expr->isInitialized()) {
        Expr *init = expr->getInitializer();
        emit(init, pointer, false);
    }

    return CValue::get(pointer);
}

CValue RecordEmitter::emit(Expr *expr, llvm::Value *dst, bool genTmp)
{
    if (AggregateExpr *agg = dyn_cast<AggregateExpr>(expr))
        return emitAggregate(agg, dst);

    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(expr))
        return emitCall(call, dst);

    if (QualifiedExpr *qual = dyn_cast<QualifiedExpr>(expr))
        return emit(qual->getOperand(), dst, genTmp);

    if (isa<DiamondExpr>(expr)) {
        RecordType *recTy = cast<RecordType>(CGR.resolveType(expr));
        return emitDefault(recTy, dst);
    }

    llvm::Value *rec = 0;

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        rec = frame()->lookup(decl, activation::Slot);
    }
    else if (IndexedArrayExpr *IAE = dyn_cast<IndexedArrayExpr>(expr)) {
        rec = CGR.emitIndexedArrayRef(IAE).first();
    }
    else if (SelectedExpr *sel = dyn_cast<SelectedExpr>(expr)) {
        rec = CGR.emitSelectedRef(sel).first();
    }
    else if (DereferenceExpr *deref = dyn_cast<DereferenceExpr>(expr)) {
        // Emit the prefix as a value, yielding a pointer to the record.  Check
        // that the result is non-null.
        rec = CGR.emitValue(deref->getPrefix()).first();
        CGR.emitNullAccessCheck(rec, deref->getLocation());
    }

    assert(rec && "Could not codegen record expression!");

    CodeGen &CG = CGR.getCodeGen();
    const llvm::Type *recTy;
    recTy = CGT.lowerType(CGR.resolveType(expr->getType()));

    // If dst is null build a stack allocated object to hold this record;
    if (dst == 0 && genTmp)
        dst = frame()->createTemp(recTy);

    // If a destination is available fill it in with the computed record data.
    if (dst) {
        llvm::Function *memcpy = CG.getMemcpy64();
        llvm::Value *source = Builder.CreatePointerCast(rec, CG.getInt8PtrTy());
        llvm::Value *target = Builder.CreatePointerCast(dst, CG.getInt8PtrTy());
        llvm::Value *len = llvm::ConstantExpr::getSizeOf(recTy);
        llvm::Value *align = llvm::ConstantInt::get(
            CG.getInt32Ty(), CG.getTargetData().getABITypeAlignment(recTy));
        Builder.CreateCall4(memcpy, target, source, len, align);
        return CValue::getRecord(dst);
    }
    else
        return CValue::getRecord(rec);
}

CValue RecordEmitter::emitAggregate(AggregateExpr *agg, llvm::Value *dst)
{
    RecordType *recTy = cast<RecordType>(agg->getType());
    const llvm::Type *loweredTy = CGT.lowerRecordType(recTy);

    // If the destination is null create a temporary to hold the aggregate.
    if (dst == 0)
        dst = frame()->createTemp(loweredTy);

    ComponentSet components;

    // Emit any positional components.
    emitPositionalComponents(agg, dst, components);

    // Emit any keyed component.
    emitKeyedComponents(agg, dst, components);

    // Emit any `others' components.
    emitOthersComponents(agg, dst, components);

    return CValue::getRecord(dst);
}

CValue RecordEmitter::emitDefault(RecordType *type, llvm::Value *dst)
{
    CodeGen &CG = CGR.getCodeGen();
    const llvm::StructType *loweredType = CGT.lowerRecordType(type);

    // If the destination is null create a temporary to hold the record.
    if (dst == 0)
        dst = frame()->createTemp(loweredType);

    // Zero initialize the record.
    uint64_t size = CGT.getTypeSize(loweredType);
    unsigned align = CGT.getTypeAlignment(loweredType);
    const llvm::Type *i32Ty = CG.getInt32Ty();
    llvm::Function *memset = CG.getMemset32();
    llvm::Value *raw = Builder.CreatePointerCast(dst, CG.getInt8PtrTy());

    Builder.CreateCall4(memset, raw,
                        llvm::ConstantInt::get(CG.getInt8Ty(), 0),
                        llvm::ConstantInt::get(i32Ty, size),
                        llvm::ConstantInt::get(i32Ty, align));

    return CValue::getRecord(dst);
}

void RecordEmitter::emitComponent(Expr *expr, llvm::Value *dst)
{
    Type *componentTy = CGR.resolveType(expr->getType());
    if (componentTy->isCompositeType())
        CGR.emitCompositeExpr(expr, dst, false);
    else if (componentTy->isFatAccessType()) {
        CValue component = CGR.emitValue(expr);
        Builder.CreateStore(Builder.CreateLoad(component.first()), dst);
    }
    else {
        CValue component = CGR.emitValue(expr);
        Builder.CreateStore(component.first(), dst);
    }
}

void RecordEmitter::emitPositionalComponents(AggregateExpr *agg, llvm::Value *dst,
                                             ComponentSet &components)
{
    if (!agg->hasPositionalComponents())
        return;

    RecordType *recTy = cast<RecordType>(agg->getType());
    RecordDecl *recDecl = recTy->getDefiningDecl();

    // Iterate over the positional components of the record declaration.  Emit
    // each component.
    typedef AggregateExpr::pos_iterator iterator;
    iterator I = agg->pos_begin();
    iterator E = agg->pos_end();
    for (unsigned i = 0; I != E; ++I, ++i) {
        ComponentDecl *component = recDecl->getComponent(i);
        unsigned index = CGT.getComponentIndex(component);
        llvm::Value *componentPtr = Builder.CreateStructGEP(dst, index);
        emitComponent(*I, componentPtr);
        components.insert(component);
    }
}

void RecordEmitter::emitKeyedComponents(AggregateExpr *agg, llvm::Value *dst,
                                        ComponentSet &components)
{
    if (!agg->hasKeyedComponents())
        return;

    typedef AggregateExpr::key_iterator iterator;
    iterator I = agg->key_begin();
    iterator E = agg->key_end();
    for ( ; I != E; ++I) {
        ComponentDecl *component = I->getAsComponent();
        unsigned index = CGT.getComponentIndex(component);
        llvm::Value *componentPtr = Builder.CreateStructGEP(dst, index);
        emitComponent(I.getExpr(), componentPtr);
        components.insert(component);
    }
}

void RecordEmitter::emitOthersComponents(AggregateExpr *agg, llvm::Value *dst,
                                         ComponentSet &components)
{
    if (!agg->hasOthers())
        return;

    // Iterate over the full set of components provided by the underlying record
    // declaration.  Emit all components that are not already present in the
    // provided set.
    Expr *others = agg->getOthersExpr();
    RecordDecl *recDecl = cast<RecordType>(agg->getType())->getDefiningDecl();
    unsigned numComponents = recDecl->numComponents();
    for (unsigned i = 0 ; i < numComponents; ++i) {
        ComponentDecl *component = recDecl->getComponent(i);
        if (!components.count(component)) {
            unsigned index = CGT.getComponentIndex(component);
            llvm::Value *componentPtr = Builder.CreateStructGEP(dst, index);
            emitComponent(others, componentPtr);
        }
    }
}

CValue RecordEmitter::emitCall(FunctionCallExpr *call, llvm::Value *dst)
{
    // Functions returning structures always use the sret calling convention.
    return CGR.emitCompositeCall(call, dst);
}

} // end anonymous namespace.

void CodeGenRoutine::emitArrayCopy(llvm::Value *source,
                                   llvm::Value *destination,
                                   ArrayType *Ty)
{
    assert(Ty->isStaticallyConstrained() && "Cannot copy unconstrained arrays!");

    // Implement array copies via memcpy.
    llvm::Value *src;
    llvm::Value *dst;
    llvm::Value *len;
    llvm::Constant *align;
    llvm::Function *memcpy;
    const llvm::PointerType *ptrTy;
    const llvm::ArrayType *arrTy;

    src = Builder.CreatePointerCast(source, CG.getInt8PtrTy());
    dst = Builder.CreatePointerCast(destination, CG.getInt8PtrTy());
    ptrTy = cast<llvm::PointerType>(source->getType());
    arrTy = cast<llvm::ArrayType>(ptrTy->getElementType());
    len = llvm::ConstantExpr::getSizeOf(arrTy);

    // Zero extend the length if not an i64.
    if (len->getType() != CG.getInt64Ty())
        len = Builder.CreateZExt(len, CG.getInt64Ty());

    align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    memcpy = CG.getMemcpy64();

    Builder.CreateCall4(memcpy, dst, src, len, align);
}

void CodeGenRoutine::emitArrayCopy(llvm::Value *source,
                                   llvm::Value *destination,
                                   llvm::Value *length,
                                   const llvm::Type *componentTy)
{
    // Scale the length by the size of the component type of the array.  The
    // current convention is that array lengths are represented as i32's.
    // Truncate the component size (an i64).
    llvm::Value *compSize;
    compSize = llvm::ConstantExpr::getSizeOf(componentTy);
    compSize = Builder.CreateTrunc(compSize, CG.getInt32Ty());
    length = Builder.CreateMul(length, compSize);

    // Finally, perform a memcpy from the source to the destination.
    //
    // FIXME: Adjust the intrinsic used to fit the target arch.
    llvm::Constant *align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    llvm::Function *memcpy = CG.getMemcpy32();

    const llvm::Type *i8PtrTy = CG.getInt8PtrTy();
    llvm::Value *src = Builder.CreatePointerCast(source, i8PtrTy);
    llvm::Value *dst = Builder.CreatePointerCast(destination, i8PtrTy);
    Builder.CreateCall4(memcpy, dst, src, length, align);
}

CValue CodeGenRoutine::emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    ArrayEmitter emitter(*this, Builder);
    return emitter.emit(expr, dst, genTmp);
}

CValue CodeGenRoutine::emitRecordExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    RecordEmitter emitter(*this, Builder);
    return emitter.emit(expr, dst, genTmp);
}

CValue
CodeGenRoutine::emitCompositeExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    Type *Ty = resolveType(expr->getType());

    if (isa<ArrayType>(Ty)) {
        ArrayEmitter emitter(*this, Builder);
        return emitter.emit(expr, dst, genTmp);
    }
    else if (isa<RecordType>(Ty)) {
        RecordEmitter emitter(*this, Builder);
        return emitter.emit(expr, dst, genTmp);
    }

    assert(false && "Not a composite expression!");
    return CValue::get(0);
}

CValue
CodeGenRoutine::emitCompositeAllocator(AllocatorExpr *expr)
{
    Type *type = resolveType(expr->getAllocatedType());

    if (isa<ArrayType>(type)) {
        ArrayEmitter emitter(*this, Builder);
        return emitter.emitAllocator(expr);
    }
    else {
        assert(isa<RecordType>(type) && "Not a composite allocator!");
        RecordEmitter emitter(*this, Builder);
        return emitter.emitAllocator(expr);
    }
}

void CodeGenRoutine::emitCompositeObjectDecl(ObjectDecl *objDecl)
{
    Type *objTy = resolveType(objDecl->getType());

    if (ArrayType *arrTy = dyn_cast<ArrayType>(objTy)) {
        BoundsEmitter emitter(*this);
        const llvm::Type *loweredTy = CGT.lowerArrayType(arrTy);

        if (!objDecl->hasInitializer()) {
            SRF->createEntry(objDecl, activation::Slot, loweredTy);
            SRF->associate(objDecl, activation::Bounds,
                           emitter.synthArrayBounds(Builder, arrTy));
            return;
        }

        llvm::Value *slot = 0;
        if (arrTy->isStaticallyConstrained())
            slot = SRF->createEntry(objDecl, activation::Slot, loweredTy);

        Expr *init = objDecl->getInitializer();
        CValue result = emitArrayExpr(init, slot, true);
        if (!slot)
            SRF->associate(objDecl, activation::Slot, result.first());
        SRF->associate(objDecl, activation::Bounds, result.second());
    }
    else {
        // We must have a record type.
        RecordType *recTy = cast<RecordType>(objTy);

        const llvm::Type *loweredTy = CGT.lowerRecordType(recTy);
        llvm::Value *slot = SRF->createEntry(objDecl, activation::Slot, loweredTy);

        // FIXME: Default initialize the records components.
        if (!objDecl->hasInitializer())
            return;

        Expr *init = objDecl->getInitializer();
        emitRecordExpr(init, slot, false);
    }
}
