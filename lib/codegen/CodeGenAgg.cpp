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

#include "comma/ast/Type.h"
#include "comma/ast/Expr.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CodeGenTypes.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void CodeGenRoutine::initArrayBounds(llvm::Value *boundSlot, ArrayType *arrTy)
{
    assert(arrTy->isConstrained() && "Expected constrained arrray!");

    llvm::Value *bounds = Builder.CreateConstGEP1_32(boundSlot, 0);
    unsigned boundIdx = 0;
    IndexConstraint *constraint = arrTy->getConstraint();
    typedef IndexConstraint::iterator iterator;

    for (iterator I = constraint->begin(); I != constraint->end(); ++I) {
        // FIXME: Only integer index types are supported ATM.
        IntegerType *idxTy = cast<IntegerType>(*I);

        // FIXME: This is wrong.  We need to evauate the actual constraint
        // bounds.
        llvm::APInt lower;
        llvm::APInt upper;
        idxTy->getLowerLimit(lower);
        idxTy->getUpperLimit(upper);

        // All bounds are represented using the root type of the index.
        const llvm::IntegerType *boundTy;
        llvm::Value *bound;
        boundTy = cast<llvm::IntegerType>(CGT.lowerType(idxTy));

        bound = Builder.CreateStructGEP(bounds, boundIdx++);
        Builder.CreateStore(CG.getConstantInt(boundTy, lower), bound);

        bound = Builder.CreateStructGEP(bounds, boundIdx++);
        Builder.CreateStore(CG.getConstantInt(boundTy, upper), bound);
    }
}

llvm::Value *CodeGenRoutine::emitArrayBounds(Expr *expr)
{
    ArrayType *arrTy = cast<ArrayType>(expr->getType());

    // If the given expression is a reference to value declaration, obtain a
    // bounds structure describing the it.
    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        if (llvm::Value *bounds = lookupBounds(ref->getDeclaration()))
            return bounds;

        // All array expressions have a constrained type except for the formal
        // parameters to a subroutine.  However, all formal parameters are
        // mapped to the bounds supplied to the function call and handled above.
        // Therefore, expr must have a constrained array type at this point.
        assert(arrTy->isConstrained() && "Unexpected unconstrained arrray!");

        // There are no bounds currently associated with this value.  Create a
        // new bound object and initialize.
        //
        // FIXME:  Initializing the bounds in the entryBB is not correct since
        // this declaration could be in an inner declarative region, but we do
        // not keep track of nested initialization scopes yet.
        llvm::Value *boundSlot = createBounds(ref->getDeclaration());
        llvm::BasicBlock *savedBB = Builder.GetInsertBlock();
        Builder.SetInsertPoint(entryBB);
        initArrayBounds(boundSlot, arrTy);
        Builder.SetInsertPoint(savedBB);
        return boundSlot;
    }

    // The expression refers to a temporary array.  Allocate a temporary to hold
    // the bounds.
    llvm::Value *boundSlot = createTemporary(CGT.lowerArrayBounds(arrTy));
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();
    Builder.SetInsertPoint(entryBB);
    initArrayBounds(boundSlot, arrTy);
    Builder.SetInsertPoint(savedBB);
    return boundSlot;
}

llvm::Value *CodeGenRoutine::emitArrayLength(ArrayType *arrTy)
{
    assert(arrTy->isConstrained() && "Unconstrained array type!");

    // FIXME: Support general discrete index types.
    IntegerType *idxTy = cast<IntegerType>(arrTy->getIndexType(0));
    llvm::Value *lower = emitScalarLowerBound(idxTy);
    llvm::Value *upper = emitScalarUpperBound(idxTy);
    llvm::Value *length = Builder.CreateSub(upper, lower);
    const llvm::IntegerType *loweredTy =
        cast<llvm::IntegerType>(length->getType());
    return Builder.CreateAdd(length, CG.getConstantInt(loweredTy, 1));
}

void CodeGenRoutine::emitArrayCopy(llvm::Value *source,
                                   llvm::Value *destination,
                                   ArrayType *Ty)
{
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

    // If the array type is of variable length, use the Comma type to compute
    // the number of elements to copy.
    if (arrTy->getNumElements() == 0)
        len = emitArrayLength(Ty);
    else
        len = llvm::ConstantExpr::getSizeOf(arrTy);

    // Zero extend the length if not an i64.
    if (len->getType() != CG.getInt64Ty())
        len = Builder.CreateZExt(len, CG.getInt64Ty());

    align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    memcpy = CG.getMemcpy64();

    Builder.CreateCall4(memcpy, dst, src, len, align);
}

llvm::Value *CodeGenRoutine::emitStringLiteral(StringLiteral *expr)
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
    //
    // FIXME:  It might be a better policy to return the constant representation
    // here and let the use sites determine what to do with the data.
    llvm::Constant *data = CG.getConstantArray(elemTy, elements);
    return CG.emitInternArray(data);
}
