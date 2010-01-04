//===-- codegen/CodeGenTypes.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CGContext.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"

#include "llvm/DerivedTypes.h"
#include "llvm/Target/TargetData.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Returns the number of elements for an array with a range bounded by the
/// given values.  If isSigned is true, treat the bounds as signed quantities,
/// otherwise as unsigned.
uint64_t getArrayWidth(const llvm::APInt &low, const llvm::APInt &high,
                       bool isSigned)
{
    llvm::APInt lower(low);
    llvm::APInt upper(high);

    // Check if this is a null range.
    if (isSigned) {
        if (upper.slt(lower))
            return 0;
    }
    else {
        if (upper.ult(lower))
            return 0;
    }

    // Compute 'upper - lower + 1' to determine the number of elements in
    // this type.
    unsigned width = std::max(lower.getBitWidth(), upper.getBitWidth()) + 1;

    if (isSigned) {
        lower.sext(width);
        upper.sext(width);
    }
    else {
        lower.zext(width);
        upper.zext(width);
    }

    llvm::APInt range(upper);
    range -= lower;
    range++;

    // Ensure the range can fit in 64 bits.
    assert(range.getActiveBits() <= 64 && "Index too wide for array type!");
    return range.getZExtValue();
}

} // end anonymous namespace.

unsigned CodeGenTypes::getTypeAlignment(const llvm::Type *type) const
{
    return CG.getTargetData().getABITypeAlignment(type);
}

uint64_t CodeGenTypes::getTypeSize(const llvm::Type *type) const
{
    return CG.getTargetData().getTypeStoreSize(type);
}

const llvm::Type *CodeGenTypes::lowerType(const Type *type)
{
    switch (type->getKind()) {

    default:
        assert(false && "Cannot lower the given Type!");
        return 0;

    case Ast::AST_DomainType:
        return lowerDomainType(cast<DomainType>(type));

    case Ast::AST_EnumerationType:
    case Ast::AST_IntegerType:
        return lowerDiscreteType(cast<DiscreteType>(type));

    case Ast::AST_ArrayType:
        return lowerArrayType(cast<ArrayType>(type));

    case Ast::AST_RecordType:
        return lowerRecordType(cast<RecordType>(type));
    }
}

void CodeGenTypes::addInstanceRewrites(const DomainInstanceDecl *instance)
{
    const FunctorDecl *functor = instance->getDefiningFunctor();
    if (!functor)
        return;

    unsigned arity = functor->getArity();
    for (unsigned i = 0; i < arity; ++i) {
        const Type *key = functor->getFormalType(i);
        const Type *value = instance->getActualParamType(i);
        rewrites.insert(key, value);
    }
}

const DomainType *
CodeGenTypes::rewriteAbstractDecl(const AbstractDomainDecl *abstract)
{
    typedef RewriteMap::iterator iterator;
    iterator I = rewrites.begin(abstract->getType());
    assert(I != rewrites.end() && "Could not resolve abstract type!");
    return cast<DomainType>(*I);
}

const llvm::Type *CodeGenTypes::lowerDomainType(const DomainType *type)
{
    if (type->isAbstract())
        type = rewriteAbstractDecl(type->getAbstractDecl());

    if (const PercentDecl *percent = type->getPercentDecl()) {
        assert(percent->getDefinition() == context->getDefinition() &&
               "Inconsistent context for PercentDecl!");
        return lowerType(context->getRepresentationType());
    }

    const DomainInstanceDecl *instance = type->getInstanceDecl();

    if (instance->isParameterized()) {
        RewriteScope scope(rewrites);
        addInstanceRewrites(instance);
        return lowerType(instance->getRepresentationType());
    }
    else
        return lowerType(instance->getRepresentationType());
}

const llvm::FunctionType *
CodeGenTypes::lowerSubroutine(const SubroutineDecl *decl)
{
    std::vector<const llvm::Type*> args;
    const llvm::Type *retTy = 0;

    if (const FunctionDecl *fdecl = dyn_cast<FunctionDecl>(decl)) {

        // If the return type is a domain, resolve to the representation type.
        const Type *targetTy = fdecl->getReturnType();
        if (const DomainType *domTy = dyn_cast<DomainType>(targetTy))
            targetTy = domTy->getRepresentationType();

        // If the return type is a statically constrained aggregate, use the
        // struct return calling convention.
        if (const CompositeType *compTy = dyn_cast<CompositeType>(targetTy)) {
            if (compTy->isConstrained()) {
                const llvm::Type *sretTy;
                sretTy = lowerType(targetTy);
                sretTy = CG.getPointerType(sretTy);
                args.push_back(sretTy);
            }

            // All arrays go thru the sret convetion or the vstack, hense a void
            // return type.
            retTy = CG.getVoidTy();
        }
        else
            retTy = lowerType(targetTy);
    }
    else
        retTy = CG.getVoidTy();

    // Emit the implicit first "%" argument, unless this is an imported
    // subroutine using the C convention.
    //
    // FIXME: All imports currently use the C convention, hence the following
    // simplistic test.
    if (!decl->findPragma(pragma::Import))
        args.push_back(CG.getRuntime().getType<CommaRT::CRT_DomainInstance>());

    SubroutineDecl::const_param_iterator I = decl->begin_params();
    SubroutineDecl::const_param_iterator E = decl->end_params();
    for ( ; I != E; ++I) {
        const ParamValueDecl *param = *I;
        const Type *paramTy = param->getType();
        const llvm::Type *loweredTy = lowerType(paramTy);

        // If the parameter type is a domain, resolve it's representation.
        if (const DomainType *domTy = dyn_cast<DomainType>(paramTy))
            paramTy = domTy->getRepresentationType();

        if (const CompositeType *compTy = dyn_cast<CompositeType>(paramTy)) {
            // We never pass composite types by value, always by reference.
            // Therefore, the parameter mode does not change how we pass the
            // argument.
            assert(isa<llvm::CompositeType>(loweredTy) &&
                   "Unexpected lowered type!");
            loweredTy = CG.getPointerType(loweredTy);

            args.push_back(loweredTy);

            // If the parameter is an unconstrained array generate an implicit
            // second argument for the bounds.
            if (const ArrayType *arrTy = dyn_cast<ArrayType>(compTy)) {
                if (!compTy->isConstrained())
                    args.push_back(CG.getPointerType(lowerArrayBounds(arrTy)));
            }
        }
        else {
            // If the argument mode is "out" or "in out", make the argument a
            // pointer-to type.
            PM::ParameterMode mode = param->getParameterMode();
            if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
                loweredTy = CG.getPointerType(loweredTy);
            args.push_back(loweredTy);
        }
    }

    return llvm::FunctionType::get(retTy, args, false);
}

const llvm::IntegerType *CodeGenTypes::lowerDiscreteType(const DiscreteType *type)
{
    return getTypeForWidth(type->getSize());
}

const llvm::ArrayType *CodeGenTypes::lowerArrayType(const ArrayType *type)
{
    assert(type->getRank() == 1 &&
           "Cannot codegen multidimensional arrays yet!");

    const llvm::Type *elementTy = lowerType(type->getComponentType());

    // If the array is unconstrained, emit a variable length array type, which
    // in LLVM is represented as an array with zero elements.
    if (!type->isConstrained())
        return llvm::ArrayType::get(elementTy, 0);

    const DiscreteType *idxTy = type->getIndexType(0);

    // Compute the bounds for the index.  If the index is itself range
    // constrained use the bounds of the range, otherwise use the bounds of the
    // root type.
    llvm::APInt lowerBound(idxTy->getSize(), 0);
    llvm::APInt upperBound(idxTy->getSize(), 0);
    if (const IntegerType *subTy = dyn_cast<IntegerType>(idxTy)) {
        if (subTy->isConstrained()) {
            if (subTy->isStaticallyConstrained()) {
                const Range *range = subTy->getConstraint();
                lowerBound = range->getStaticLowerBound();
                upperBound = range->getStaticUpperBound();
            }
            else
                return llvm::ArrayType::get(elementTy, 0);
        }
        else {
            const IntegerType *rootTy = subTy->getRootType();
            rootTy->getLowerLimit(lowerBound);
            rootTy->getUpperLimit(upperBound);
        }
    }
    else {
        // FIXME: Use range constraints here.
        const EnumerationType *enumTy = cast<EnumerationType>(idxTy);
        assert(enumTy && "Unexpected array index type!");
        lowerBound = 0;
        upperBound = enumTy->getNumLiterals() - 1;
    }

    uint64_t numElems;
    numElems = getArrayWidth(lowerBound, upperBound, idxTy->isSigned());
    return llvm::ArrayType::get(elementTy, numElems);
}

const llvm::StructType *CodeGenTypes::lowerRecordType(const RecordType *recTy)
{
    unsigned maxAlignment = 0;
    uint64_t currentOffset = 0;
    uint64_t requiredOffset = 0;
    unsigned currentIndex = 0;
    std::vector<const llvm::Type*> fields;

    const RecordDecl *recDecl = recTy->getDefiningDecl();
    for (unsigned i = 0; i < recDecl->numComponents(); ++i) {
        const ComponentDecl *componentDecl = recDecl->getComponent(i);
        const llvm::Type *componentTy = lowerType(componentDecl->getType());
        unsigned alignment = getTypeAlignment(componentTy);
        requiredOffset = llvm::TargetData::RoundUpAlignment(currentOffset,
                                                            alignment);
        maxAlignment = std::max(maxAlignment, alignment);

        // Output as many i8's as needed to bring the current offset upto the
        // required offset, then emit the actual field.
        while (currentOffset < requiredOffset) {
            fields.push_back(CG.getInt8Ty());
            currentOffset++;
            currentIndex++;
        }
        fields.push_back(componentTy);
        ComponentIndices[componentDecl] = currentIndex;
        currentOffset = requiredOffset + getTypeSize(componentTy);
        currentIndex++;
    }

    // Pad out the record if needed.
    requiredOffset = llvm::TargetData::RoundUpAlignment(currentOffset,
                                                        maxAlignment);
    while (currentOffset < requiredOffset) {
        fields.push_back(CG.getInt8Ty());
        currentOffset++;
    }

    return llvm::StructType::get(CG.getLLVMContext(), fields);
}

unsigned CodeGenTypes::getComponentIndex(const ComponentDecl *component)
{
    ComponentIndexMap::iterator I = ComponentIndices.find(component);
    assert (I != ComponentIndices.end()  && "Component index does not exist!");
    return I->second;
}

const llvm::StructType *CodeGenTypes::lowerArrayBounds(const ArrayType *arrTy)
{
    std::vector<const llvm::Type*> elts;
    const ArrayType *baseTy = arrTy->getRootType();

    for (unsigned i = 0; i < baseTy->getRank(); ++i) {
        const llvm::Type *boundTy = lowerType(baseTy->getIndexType(i));
        elts.push_back(boundTy);
        elts.push_back(boundTy);
    }

    return CG.getStructTy(elts);
}

const llvm::StructType *
CodeGenTypes::lowerScalarBounds(const DiscreteType *type)
{
    std::vector<const llvm::Type*> elts;
    const llvm::Type *elemTy = lowerType(type);
    elts.push_back(elemTy);
    elts.push_back(elemTy);
    return CG.getStructTy(elts);
}

const llvm::StructType *CodeGenTypes::lowerRange(const Range *range)
{
    std::vector<const llvm::Type*> elts;
    const llvm::Type *elemTy = lowerType(range->getType());
    elts.push_back(elemTy);
    elts.push_back(elemTy);
    return CG.getStructTy(elts);
}

const llvm::IntegerType *CodeGenTypes::getTypeForWidth(unsigned numBits)
{
    // Promote the bit width to a power of two.
    if (numBits <= 1)
        return CG.getInt1Ty();
    if (numBits <= 8)
        return CG.getInt8Ty();
    else if (numBits <= 16)
        return CG.getInt16Ty();
    else if (numBits <= 32)
        return CG.getInt32Ty();
    else if (numBits <= 64)
        return CG.getInt64Ty();
    else {
        // FIXME: This should be a fatal error, not an assert.
        assert(false && "Bit size too large to codegen!");
        return 0;
    }
}
