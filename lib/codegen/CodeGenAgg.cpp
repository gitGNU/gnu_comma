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

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

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
                                   llvm::Value *length)
{
    // Scale the length by the size of the component type of the arrays.
    const llvm::SequentialType *seqTy;
    const llvm::Type *compTy;
    llvm::Value *compSize;
    seqTy = cast<llvm::PointerType>(destination->getType());
    seqTy = cast<llvm::ArrayType>(seqTy->getElementType());
    compTy = seqTy->getElementType();
    compSize = llvm::ConstantExpr::getSizeOf(compTy);

    // The current convention is that array lengths are represented as i32's.
    // Truncate the component size (an i64).
    compSize = Builder.CreateTrunc(compSize, CG.getInt32Ty());
    length = Builder.CreateMul(length, compSize);

    // Finally, perform a memcpy from the source to the destination.
    //
    // FIXME: Adjust the intrinsic used to fit the target arch.
    llvm::Constant *align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    llvm::Function *memcpy = CG.getMemcpy32();

    seqTy = CG.getInt8PtrTy();
    llvm::Value *src = Builder.CreatePointerCast(source, seqTy);
    llvm::Value *dst = Builder.CreatePointerCast(destination, seqTy);
    Builder.CreateCall4(memcpy, dst, src, length, align);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitStringLiteral(StringLiteral *expr)
{
    assert(expr->hasType() && "Unresolved string literal type!");

    // Build a constant array representing the literal.
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
    llvm::Constant *arrData = CG.getConstantArray(elemTy, elements);
    llvm::GlobalVariable *dataPtr =
        new llvm::GlobalVariable(*CG.getModule(), arrData->getType(), true,
                                 llvm::GlobalValue::InternalLinkage, arrData,
                                 "string.data");

    // Likewise, build a constant global to represent the bounds.  Emitting the
    // bounds into memory should be slightly faster then building them on the
    // stack in the majority of cases.
    BoundsEmitter emitter(*this);
    llvm::Constant *boundData =
        emitter.synthStaticArrayBounds(Builder, expr->getType());
    llvm::GlobalVariable *boundPtr =
        new llvm::GlobalVariable(*CG.getModule(), boundData->getType(), true,
                                 llvm::GlobalValue::InternalLinkage,
                                 boundData, "bounds.data");
    return std::pair<llvm::Value*, llvm::Value*>(dataPtr, boundPtr);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    typedef std::pair<llvm::Value*, llvm::Value*> ArrPair;

    llvm::Value *components;
    llvm::Value *bounds;
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    BoundsEmitter emitter(*this);

    if (ConversionExpr *convert = dyn_cast<ConversionExpr>(expr)) {
        ArrPair result = emitArrayExpr(convert->getOperand(), dst, genTmp);
        emitArrayConversion(convert, result.first, result.second);
        return result;
    }

    if (StringLiteral *lit = dyn_cast<StringLiteral>(expr)) {
        ArrPair pair = emitStringLiteral(lit);
        components = pair.first;
        bounds = pair.second;
    }
    else if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        components = lookupDecl(decl);

        /// FIXME: Bounds lookup will fail when the decl is a ParamValueDecl of
        /// unconstrained type.  A better API will replace this hack.
        if (!(bounds = lookupBounds(decl)))
            bounds = emitter.synthStaticArrayBounds(Builder, arrTy);
    }
    else if (AggregateExpr *agg = dyn_cast<AggregateExpr>(expr))
        return emitAggregate(agg, dst, genTmp);
    else {
        assert(false && "Invalid type of array expr!");
        return ArrPair(0, 0);
    }

    // If dst is null build a stack allocated object to hold the components of
    // this array.
    if (dst == 0 && genTmp) {
        llvm::BasicBlock *savedBB;
        const llvm::Type *componentTy;
        const llvm::Type *dstTy;
        llvm::Value *length;

        savedBB = Builder.GetInsertBlock();
        Builder.SetInsertPoint(entryBB);

        componentTy = CGT.lowerType(arrTy->getComponentType());
        dstTy = CG.getPointerType(CG.getVLArrayTy(componentTy));

        if (isa<llvm::PointerType>(bounds->getType()))
            bounds = Builder.CreateLoad(bounds);
        length = emitter.computeTotalBoundLength(Builder, bounds);
        dst = Builder.CreateAlloca(componentTy, length);
        dst = Builder.CreatePointerCast(dst, dstTy);
        Builder.SetInsertPoint(savedBB);
    }

    // If a destination is available, fill it in with the associated array data.
    if (isa<llvm::PointerType>(bounds->getType()))
        bounds = Builder.CreateLoad(bounds);
    if (dst) {
        llvm::Value *length = emitter.computeTotalBoundLength(Builder, bounds);
        emitArrayCopy(components, dst, length);
        return ArrPair(dst, bounds);
    }
    else
        return ArrPair(components, bounds);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitAggregate(AggregateExpr *expr, llvm::Value *dst,
                              bool genTmp)
{
    typedef std::pair<llvm::Value*, llvm::Value*> ArrPair;
    typedef AggregateExpr::component_iter iterator;

    BoundsEmitter emitter(*this);
    llvm::Value *bounds = emitter.synthAggregateBounds(Builder, expr);
    ArrayType *arrTy = cast<ArrayType>(expr->getType());

    std::vector<llvm::Value*> components;

    if (!dst && genTmp) {
        llvm::Value *length = emitter.computeBoundLength(Builder, bounds, 0);
        const llvm::Type *elemTy = CGT.lowerType(arrTy->getComponentType());
        const llvm::Type *dstTy = CG.getPointerType(CG.getVLArrayTy(elemTy));
        dst = Builder.CreateAlloca(elemTy, length);
        dst = Builder.CreatePointerCast(dst, dstTy);
    }

    iterator I = expr->begin_components();
    iterator E = expr->end_components();
    for ( ; I != E; ++I)
        components.push_back(emitValue(*I));

    for (unsigned i = 0; i < components.size(); ++i) {
        llvm::Value *idx = Builder.CreateConstGEP2_64(dst, 0, i);
        Builder.CreateStore(components[i], idx);
    }

    return ArrPair(dst, bounds);
}

void CodeGenRoutine::emitArrayConversion(ConversionExpr *conver,
                                         llvm::Value *components,
                                         llvm::Value *bounds)
{
    // FIXME: Implement.
}
