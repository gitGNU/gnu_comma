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
#include "CGContext.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
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

private:
    CodeGenRoutine &CGR;
    BoundsEmitter emitter;
    llvm::IRBuilder<> &Builder;

    SRFrame *frame() { return CGR.getSRFrame(); }

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
    CValue emitOthersKeyedAgg(AggregateExpr *expr, llvm::Value *dst);

    CValue emitArrayConversion(ConversionExpr *convert, llvm::Value *dst,
                               bool genTmp);
    CValue emitCall(FunctionCallExpr *call, llvm::Value *dst);
    CValue emitAggregate(AggregateExpr *expr, llvm::Value *dst);
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
};

llvm::Value *ArrayEmitter::convertIndex(llvm::Value *idx)
{
    const llvm::Type *intptrTy = CGR.getCodeGen().getIntPtrTy();
    if (idx->getType() != intptrTy)
        return Builder.CreateIntCast(idx, intptrTy, false);
    return idx;
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

    if (InjExpr *inj = dyn_cast<InjExpr>(expr))
        return emit(inj->getOperand(), dst, genTmp);

    if (PrjExpr *prj = dyn_cast<PrjExpr>(expr))
        return emit(prj->getOperand(), dst, genTmp);

    if (QualifiedExpr *qual = dyn_cast<QualifiedExpr>(expr))
        return emit(qual->getOperand(), dst, genTmp);

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
        components = CGR.emitIndexedArrayRef(iae);
        bounds = emitter.synthArrayBounds(Builder, arrTy);
    }
    else if (SelectedExpr *sel = dyn_cast<SelectedExpr>(expr)) {
        components = CGR.emitSelectedRef(sel);
        bounds = emitter.synthArrayBounds(Builder, arrTy);
    }
    else {
        assert(false && "Invalid type of array expr!");
        return CValue::getAgg(0, 0);
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
        return CValue::getAgg(dst, bounds);
    }
    else
        return CValue::getAgg(components, bounds);
}

void ArrayEmitter::emitComponent(Expr *expr, llvm::Value *dst)
{
    if (isa<CompositeType>(expr->getType()))
        CGR.emitCompositeExpr(expr, dst, false);
    else
        Builder.CreateStore(CGR.emitValue(expr).first(), dst);
}

CValue ArrayEmitter::emitCall(FunctionCallExpr *call, llvm::Value *dst)
{
    ArrayType *arrTy = cast<ArrayType>(CGR.resolveType(call->getType()));

    // Constrained types use the sret call convention.
    if (arrTy->isConstrained()) {
        dst = CGR.emitCompositeCall(call, dst);
        llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);
        return CValue::getAgg(dst, bounds);
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

CValue ArrayEmitter::emitStringLiteral(StringLiteral *expr)
{
    assert(expr->hasType() && "Unresolved string literal type!");

    // Build a constant array representing the literal.
    CodeGen &CG = CGR.getCodeGen();
    CodeGenTypes &CGT = CGR.getCGC().getCGT();
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
    return CValue::getAgg(dataPtr, boundPtr);
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

    // Populate the phi node this the incoming values.
    phi->addIncoming(iterStart, startBB);
    phi->addIncoming(iterNext, bodyBB);

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
    return CValue::getAgg(dst, bounds);
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

    return CValue::getAgg(dst, bounds);
}

void ArrayEmitter::fillInOthers(AggregateExpr *agg, llvm::Value *dst,
                                llvm::Value *lower, llvm::Value *upper)
{
    // If this aggregate does not specify an others component we are done.
    //
    // FIXME: If an undefined others component is specified we should be
    // initializing the components with their default value (if any).
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

CValue ArrayEmitter::emitDynamicKeyedAgg(AggregateExpr *expr, llvm::Value *dst)
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
    return CValue::getAgg(dst, bounds);
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
    return CValue::getAgg(dst, bounds);
}

llvm::Value *ArrayEmitter::allocArray(ArrayType *arrTy, llvm::Value *bounds,
                                      llvm::Value *&dst)
{
    CodeGenTypes &CGT = CGR.getCGC().getCGT();

    if (arrTy->isStaticallyConstrained()) {
        dst = frame()->createTemp(CGT.lowerArrayType(arrTy));
        return 0;
    }
    else {
        CodeGen &CG = CGR.getCodeGen();
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
          CGT(CGR.getCGC().getCGT()),
          emitter(CGR),
          Builder(Builder) { }

    llvm::Value *emit(Expr *expr, llvm::Value *dst, bool genTmp);

private:
    CodeGenRoutine &CGR;
    CodeGenTypes &CGT;
    BoundsEmitter emitter;
    llvm::IRBuilder<> &Builder;

    SRFrame *frame() { return CGR.getSRFrame(); }

    /// Emits a function call expression returning a record type as result.
    llvm::Value *emitCall(FunctionCallExpr *call, llvm::Value *dst);

    /// Emits an aggregate expression of record type.
    ///
    /// \param agg The aggregate to emit.
    ///
    /// \param dst A pointer to a corresponding llvm structure to hold the
    /// result of the aggregate or null.  When \p dst is null a temporary will
    /// be allocated to hold the aggregate.
    ///
    /// \return A pointer to an llvm structure populated with the components
    /// defined by \p agg.
    llvm::Value *emitAggregate(AggregateExpr *agg, llvm::Value *dst);

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

llvm::Value *RecordEmitter::emit(Expr *expr, llvm::Value *dst, bool genTmp)
{
    if (AggregateExpr *agg = dyn_cast<AggregateExpr>(expr))
        return emitAggregate(agg, dst);

    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(expr))
        return emitCall(call, dst);

    if (InjExpr *inj = dyn_cast<InjExpr>(expr))
        return emit(inj->getOperand(), dst, genTmp);

    if (PrjExpr *prj = dyn_cast<PrjExpr>(expr))
        return emit(prj->getOperand(), dst, genTmp);

    if (QualifiedExpr *qual = dyn_cast<QualifiedExpr>(expr))
        return emit(qual->getOperand(), dst, genTmp);

    llvm::Value *rec = 0;

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        rec = frame()->lookup(decl, activation::Slot);
    }
    else if (IndexedArrayExpr *IAE = dyn_cast<IndexedArrayExpr>(expr)) {
        rec = CGR.emitIndexedArrayRef(IAE);
    }
    else if (SelectedExpr *sel = dyn_cast<SelectedExpr>(expr)) {
        rec = CGR.emitSelectedRef(sel);
    }
    else if (DereferenceExpr *deref = dyn_cast<DereferenceExpr>(expr)) {
        // Emit the prefix as a value, yielding a pointer to the record.
        rec = CGR.emitValue(deref->getPrefix()).first();
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
        return dst;
    }
    else
        return rec;
}

llvm::Value *RecordEmitter::emitAggregate(AggregateExpr *agg, llvm::Value *dst)
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

    return dst;
}

void RecordEmitter::emitComponent(Expr *expr, llvm::Value *dst)
{
    if (isa<CompositeType>(CGR.resolveType(expr->getType())))
        CGR.emitCompositeExpr(expr, dst, false);
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

    // FIXME:  We should be default initializing components if there is no
    // expression associated with the others clause.
    Expr *others = agg->getOthersExpr();
    if (!others)
        return;

    // Iterate over the full set of components provided by the underlying record
    // declaration.  Emit all components that are not already present in the
    // provided set.
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

llvm::Value *RecordEmitter::emitCall(FunctionCallExpr *call, llvm::Value *dst)
{
    // Functions returning structures always use the sret calling convention.
    //
    // FIXME: Optimize the case when the record is small.
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

llvm::Value *
CodeGenRoutine::emitRecordExpr(Expr *expr, llvm::Value *dst, bool genTmp)
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
        return CValue::getSimple(emitter.emit(expr, dst, genTmp));
    }

    assert(false && "Not a composite expression!");
    return CValue::getSimple(0);
}

void CodeGenRoutine::emitCompositeObjectDecl(ObjectDecl *objDecl)
{
    Type *objTy = resolveType(objDecl->getType());

    if (ArrayType *arrTy = dyn_cast<ArrayType>(objTy)) {
        BoundsEmitter emitter(*this);
        const llvm::Type *loweredTy = CGT.lowerArrayType(arrTy);

        if (!objDecl->hasInitializer()) {
            // FIXME: Support dynamicly sized arrays and default initialization.
            assert(arrTy->isStaticallyConstrained() &&
                   "Cannot codegen non-static arrays without initializer!");

            SRF->createEntry(objDecl, activation::Slot, loweredTy);
            SRF->associate(objDecl, activation::Bounds,
                           emitter.synthStaticArrayBounds(Builder, arrTy));
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
