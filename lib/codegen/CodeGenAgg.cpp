//===-- codegen/CodeGenAgg.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
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
#include "comma/ast/Type.h"
#include "comma/ast/Expr.h"

#include <algorithm>

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// \class
///
/// \brief Helper class for aggregate emission.
class AggEmitter {

public:
    AggEmitter(CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder)
        : CGR(CGR),
          emitter(CGR),
          Builder(Builder) { }

    typedef std::pair<llvm::Value *, llvm::Value *> ValuePair;

    ValuePair emit(Expr *expr, llvm::Value *dst, bool genTmp);

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
    void fillInOthers(KeyedAggExpr *agg, llvm::Value *dst,
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

    ValuePair emitPositionalAgg(PositionalAggExpr *expr, llvm::Value *dst);
    ValuePair emitKeyedAgg(KeyedAggExpr *expr, llvm::Value *dst);
    ValuePair emitStaticKeyedAgg(KeyedAggExpr *expr, llvm::Value *dst);
    ValuePair emitDynamicKeyedAgg(KeyedAggExpr *expr, llvm::Value *dst);
    ValuePair emitOthersKeyedAgg(KeyedAggExpr *expr, llvm::Value *dst);

    ValuePair emitArrayConversion(ConversionExpr *convert, llvm::Value *dst,
                                  bool genTmp);
    ValuePair emitCall(FunctionCallExpr *call, llvm::Value *dst);
    ValuePair emitAggregate(AggregateExpr *expr, llvm::Value *dst);
    ValuePair emitStringLiteral(StringLiteral *expr);

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

    /// Emits the components defined by a descrete choice.
    ///
    /// \param I Iterator yielding the component to emit.
    ///
    /// \param dst Pointer to storage sufficient to hold the aggregate data.
    ///
    /// \param bias The lower bound of the associated discrete index type.  This
    /// value is used to correct the actual index values defined by the discrete
    /// choice so that they are zero based.
    void emitDiscreteComponent(KeyedAggExpr::choice_iterator &I,
                               llvm::Value *dst, llvm::Value *bias);

    /// \brief Emits the array component given by \p expr and stores the result
    /// in \p dst.
    void emitComponent(Expr *expr, llvm::Value *dst);

    /// Converts the given value to an unsigned pointer sized integer if needed.
    llvm::Value *convertIndex(llvm::Value *idx);
};

llvm::Value *AggEmitter::convertIndex(llvm::Value *idx)
{
    const llvm::Type *intptrTy = CGR.getCodeGen().getIntPtrTy();
    if (idx->getType() != intptrTy)
        return Builder.CreateIntCast(idx, intptrTy, false);
    return idx;
}

AggEmitter::ValuePair
AggEmitter::emit(Expr *expr, llvm::Value *dst, bool genTmp)
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

    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        components = frame()->lookup(decl, activation::Slot);

        /// FIXME: Bounds lookup will fail when the decl is a ParamValueDecl of
        /// a statically constrained type.  A better API will replace this hack.
        if (!(bounds = frame()->lookup(decl, activation::Bounds)))
            bounds = emitter.synthStaticArrayBounds(Builder, arrTy);
    }
    else if (StringLiteral *lit = dyn_cast<StringLiteral>(expr)) {
        ValuePair pair = emitStringLiteral(lit);
        components = pair.first;
        bounds = pair.second;
    }
    else if (IndexedArrayExpr *iae = dyn_cast<IndexedArrayExpr>(expr)) {
        components = CGR.emitIndexedArrayRef(iae);
        bounds = emitter.synthArrayBounds(Builder, arrTy);
    }
    else {
        assert(false && "Invalid type of array expr!");
        return ValuePair(0, 0);
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
        return ValuePair(dst, bounds);
    }
    else
        return ValuePair(components, bounds);
}

void AggEmitter::emitComponent(Expr *expr, llvm::Value *dst)
{
    if (isa<ArrayType>(expr->getType()))
        CGR.emitArrayExpr(expr, dst, false);
    else
        Builder.CreateStore(CGR.emitValue(expr), dst);
}

AggEmitter::ValuePair AggEmitter::emitCall(FunctionCallExpr *call,
                                           llvm::Value *dst)
{
    ArrayType *arrTy = cast<ArrayType>(call->getType());

    // Constrained types use the sret call convention.
    if (arrTy->isConstrained()) {
        dst = CGR.emitCompositeCall(call, dst);
        llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);
        return std::pair<llvm::Value*, llvm::Value*>(dst, bounds);
    }

    // Unconstrained (indefinite type) use the vstack.  The callee cannot know
    // the size of the result hense dst must be null.
    assert(dst == 0 && "Destination given for indefinite type!");
    return CGR.emitVStackCall(call);
}

AggEmitter::ValuePair AggEmitter::emitAggregate(AggregateExpr *expr,
                                                llvm::Value *dst)
{
    if (PositionalAggExpr *PAE = dyn_cast<PositionalAggExpr>(expr))
        return emitPositionalAgg(PAE, dst);

    KeyedAggExpr *KAE = cast<KeyedAggExpr>(expr);
    return emitKeyedAgg(KAE, dst);
}

AggEmitter::ValuePair AggEmitter::emitStringLiteral(StringLiteral *expr)
{
    assert(expr->hasType() && "Unresolved string literal type!");

    // Build a constant array representing the literal.
    CodeGen &CG = CGR.getCodeGen();
    CodeGenTypes &CGT = CGR.getCGC().getTypeGenerator();
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
    return ValuePair(dataPtr, boundPtr);
}

AggEmitter::ValuePair
AggEmitter::emitArrayConversion(ConversionExpr *convert, llvm::Value *dst,
                                bool genTmp)
{
    // FIXME: Implement.
    return emit(convert->getOperand(), dst, genTmp);
}

void AggEmitter::emitOthers(Expr *others, llvm::Value *dst,
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

void AggEmitter::emitOthers(AggregateExpr *expr,
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

AggEmitter::ValuePair AggEmitter::emitPositionalAgg(PositionalAggExpr *expr,
                                                    llvm::Value *dst)
{
    llvm::Value *bounds = emitter.synthAggregateBounds(Builder, expr);
    ArrayType *arrTy = cast<ArrayType>(expr->getType());

    std::vector<llvm::Value*> components;

    if (dst == 0)
        allocArray(arrTy, bounds, dst);

    typedef PositionalAggExpr::iterator iterator;
    iterator I = expr->begin_components();
    iterator E = expr->end_components();
    for (unsigned idx = 0; I != E; ++I, ++idx)
        emitComponent(*I, Builder.CreateConstGEP2_32(dst, 0, idx));

    // Emit an "others" clause if present.
    emitOthers(expr, dst, bounds, components.size());
    return ValuePair(dst, bounds);
}

AggEmitter::ValuePair AggEmitter::emitKeyedAgg(KeyedAggExpr *expr,
                                               llvm::Value *dst)
{
    if (expr->hasStaticIndices())
        return emitStaticKeyedAgg(expr, dst);

    if (expr->numChoices() == 1)
        return emitDynamicKeyedAgg(expr, dst);

    assert(expr->numChoices() == 0);
    assert(expr->hasOthers());

    return emitOthersKeyedAgg(expr, dst);
}

void AggEmitter::emitDiscreteComponent(KeyedAggExpr::choice_iterator &I,
                                       llvm::Value *dst, llvm::Value *bias)
{
    // FIXME: Only ranges are supported at the moment.
    Range *range = cast<Range>(*I);
    uint64_t length = range->length();
    Expr *expr = I.getExpr();
    llvm::Value *idx = emitter.getLowerBound(Builder, range);
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

AggEmitter::ValuePair
AggEmitter::emitStaticKeyedAgg(KeyedAggExpr *agg, llvm::Value *dst)
{
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
    KeyedAggExpr::choice_iterator I = agg->choice_begin();
    KeyedAggExpr::choice_iterator E = agg->choice_end();
    for ( ; I != E; ++I)
        emitDiscreteComponent(I, dst, lower);
    fillInOthers(agg, dst, lower, upper);

    return ValuePair(dst, bounds);
}

void AggEmitter::fillInOthers(KeyedAggExpr *agg, llvm::Value *dst,
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

    // Build a sorted vector of the choices supplied by the aggregate.
    typedef std::vector<Ast*> ChoiceVec;
    ChoiceVec CV(agg->choice_begin(), agg->choice_end());
    if (idxTy->isSigned())
        std::sort(CV.begin(), CV.end(), KeyedAggExpr::compareChoicesS);
    else
        std::sort(CV.begin(), CV.end(), KeyedAggExpr::compareChoicesU);

    llvm::APInt limit;
    Range *choice;
    const llvm::Type *iterTy = lower->getType();

    // Fill in any missing leading elements.
    choice = cast<Range>(CV.front());
    idxTy->getLowerLimit(limit);
    if (choice->getStaticLowerBound() != limit) {
        llvm::Value *end = CGR.emitValue(choice->getLowerBound());
        emitOthers(others, dst, lower, end, lower);
    }

    // Fill in each interior "hole".
    for (unsigned i = 0; i < CV.size() - 1; ++i) {
        llvm::APInt lo = cast<Range>(CV[i])->getStaticUpperBound();
        const llvm::APInt &hi = cast<Range>(CV[i+1])->getStaticLowerBound();

        if (lo == hi)
            continue;

        llvm::Value *start = llvm::ConstantInt::get(iterTy, ++lo);
        llvm::Value *end = llvm::ConstantInt::get(iterTy, hi);
        emitOthers(others, dst, start, end, lower);
    }

    // Fill in any missing trailing elements.
    choice = cast<Range>(CV.back());
    idxTy->getUpperLimit(limit);
    if (choice->getStaticUpperBound() != limit) {
        llvm::Value *start;
        llvm::Value *end;
        start = CGR.emitValue(choice->getUpperBound());
        start = Builder.CreateAdd(start, llvm::ConstantInt::get(iterTy, 1));
        end = Builder.CreateAdd(upper, llvm::ConstantInt::get(iterTy, 1));
        emitOthers(others, dst, start, end, lower);
    }
}

AggEmitter::ValuePair
AggEmitter::emitDynamicKeyedAgg(KeyedAggExpr *expr, llvm::Value *dst)
{
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    KeyedAggExpr::choice_iterator I = expr->choice_begin();
    Range *range = cast<Range>(*I);
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
    return ValuePair(dst, bounds);
}

AggEmitter::ValuePair
AggEmitter::emitOthersKeyedAgg(KeyedAggExpr *expr, llvm::Value *dst)
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
    return ValuePair(dst, bounds);
}

llvm::Value *AggEmitter::allocArray(ArrayType *arrTy, llvm::Value *bounds,
                                    llvm::Value *&dst)
{
    CodeGenTypes &CGT = CGR.getCGC().getTypeGenerator();

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

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    AggEmitter emitter(*this, Builder);
    return emitter.emit(expr, dst, genTmp);
}

void CodeGenRoutine::emitArrayObjectDecl(ObjectDecl *objDecl)
{
    BoundsEmitter emitter(*this);
    ArrayType *arrTy = cast<ArrayType>(resolveType(objDecl->getType()));
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
    std::pair<llvm::Value*, llvm::Value*> result;

    result = emitArrayExpr(init, slot, true);
    if (!slot)
        SRF->associate(objDecl, activation::Slot, result.first);
    SRF->associate(objDecl, activation::Bounds, result.second);
}
