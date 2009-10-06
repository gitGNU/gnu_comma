//===-- codegen/CodeGenTypes.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/DerivedTypes.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/MathExtras.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

const llvm::Type *CodeGenTypes::lowerType(const Type *type)
{
    if (const SubType *subtype = dyn_cast<SubType>(type))
        return lowerSubType(subtype);

    switch (type->getKind()) {

    default:
        assert(false && "Cannot lower the given Type!");
        return 0;

    case Ast::AST_DomainType:
        return lowerDomainType(cast<DomainType>(type));

    case Ast::AST_EnumerationType:
        return lowerEnumType(cast<EnumerationType>(type));

    case Ast::AST_IntegerType:
        return lowerIntegerType(cast<IntegerType>(type));

    case Ast::AST_ArrayType:
        return lowerArrayType(cast<ArrayType>(type));
    }
}

void CodeGenTypes::addInstanceRewrites(DomainInstanceDecl *instance)
{
    FunctorDecl *functor = instance->getDefiningFunctor();
    if (!functor) return;

    typedef RewriteMap::value_type KeyVal;
    unsigned arity = functor->getArity();
    for (unsigned i = 0; i < arity; ++i) {
        KeyVal &KV = rewrites.FindAndConstruct(functor->getFormalType(i));
        RewriteVal &RV = KV.second;
        RV.first = instance->getActualParamType(i);
        RV.second++;
    }
}

void CodeGenTypes::removeInstanceRewrites(DomainInstanceDecl *instance)
{
    FunctorDecl *functor = instance->getDefiningFunctor();
    if (!functor) return;

    typedef RewriteMap::iterator iterator;
    unsigned arity = functor->getArity();
    for (unsigned i = 0; i < arity; ++i) {
        iterator I = rewrites.find(functor->getFormalType(i));
        assert(I != rewrites.end() && "Inconsistent rewrites!");
        RewriteVal &RV = I->second;
        if (--RV.second == 0)
            rewrites.erase(I);
    }
}

const DomainType *CodeGenTypes::rewriteAbstractDecl(AbstractDomainDecl *abstract)
{
    // Check the rewrite map, followed by parameter map given by the
    // current capsule generator.
    Type *source = abstract->getType();

    {
        typedef RewriteMap::iterator iterator;
        iterator I = rewrites.find(source);
        if (I != rewrites.end()) {
            const RewriteVal &RV = I->second;
            return cast<DomainType>(RV.first);
        }
    }

    {
        typedef CodeGenCapsule::ParameterMap ParamMap;
        const ParamMap &pMap = CG.getCapsuleGenerator().getParameterMap();
        ParamMap::const_iterator I = pMap.find(source);
        assert(I != pMap.end() && "Could not resolve abstract type!");
        return cast<DomainType>(I->second);
    }
}

const llvm::Type *CodeGenTypes::lowerDomainType(const DomainType *type)
{
    if (type->isAbstract())
        type = rewriteAbstractDecl(type->getAbstractDecl());

    if (PercentDecl *percent = type->getPercentDecl()) {
        const Domoid *domoid = cast<Domoid>(percent->getDefinition());
        return lowerDomoidCarrier(domoid);
    }
    else {
        // This must correspond to an instance decl.  Resolve the carrier type.
        DomainInstanceDecl *instance = type->getInstanceDecl();
        assert(instance && "Cannot lower this kind of type!");

        addInstanceRewrites(instance);
        const llvm::Type *result
            = lowerDomoidCarrier(instance->getDefinition());
        removeInstanceRewrites(instance);
        return result;
    }
}

const llvm::Type *CodeGenTypes::lowerDomoidCarrier(const Domoid *domoid)
{
    const AddDecl *add = domoid->getImplementation();
    assert(add->hasCarrier() && "Cannot codegen domains without carriers!");
    return lowerCarrierType(add->getCarrier()->getType());
}

const llvm::Type *CodeGenTypes::lowerCarrierType(const CarrierType *type)
{
    return lowerType(type->getTypeOf());
}

const llvm::IntegerType *
CodeGenTypes::lowerEnumType(const EnumerationType *type)
{
    // Enumeration types are lowered to an integer type with sufficient capacity
    // to hold each element of the enumeration.
    unsigned numBits = llvm::Log2_32_Ceil(type->getNumElements());
    return getTypeForWidth(numBits);
}

const llvm::FunctionType *
CodeGenTypes::lowerSubroutine(const SubroutineDecl *decl)
{
    std::vector<const llvm::Type*> args;
    const llvm::Type *retTy = 0;

    // If the return type is an aggregate, use the struct return calling
    // convention.
    if (const FunctionDecl *fdecl = dyn_cast<FunctionDecl>(decl)) {
        retTy = lowerType(fdecl->getReturnType());
        if (isa<llvm::StructType>(retTy) || isa<llvm::ArrayType>(retTy)) {
            args.push_back(CG.getPointerType(retTy));
            retTy = CG.getVoidTy();
        }
    }
    else
        retTy = CG.getVoidTy();

    // Emit the implicit first "%" argument.
    args.push_back(CG.getRuntime().getType<CommaRT::CRT_DomainInstance>());

    SubroutineDecl::const_param_iterator I = decl->begin_params();
    SubroutineDecl::const_param_iterator E = decl->end_params();
    for ( ; I != E; ++I) {
        const ParamValueDecl *param = *I;
        const Type *paramTy = param->getType();
        const llvm::Type *loweredTy = lowerType(paramTy);

        if (const ArraySubType *arrTy = dyn_cast<ArraySubType>(paramTy)) {
            // We never pass arrays by value, always by reference.  Therefore,
            // the parameter mode does not change how we pass an array.
            assert(isa<llvm::ArrayType>(loweredTy) &&
                   "Unexpected type for array!");
            loweredTy = CG.getPointerType(loweredTy);

            args.push_back(loweredTy);

            // If the array is unconstrained, generate an implicit second
            // argument for the bounds.
            if (!arrTy->isConstrained())
                args.push_back(CG.getPointerType(lowerArrayBounds(arrTy)));
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

const llvm::Type *CodeGenTypes::lowerSubType(const SubType *type)
{
    if (const ArraySubType *arrayTy = dyn_cast<ArraySubType>(type))
        return lowerArraySubType(arrayTy);

    return lowerType(type->getTypeOf());
}

const llvm::IntegerType *CodeGenTypes::lowerIntegerType(const IntegerType *type)
{
    return getTypeForWidth(type->getBitWidth());
}

const llvm::ArrayType *CodeGenTypes::lowerArrayType(const ArrayType *type)
{
    assert(type->getRank() == 1 &&
           "Cannot codegen multidimensional arrays yet!");

    Type *indexType = type->getIndexType(0);
    uint64_t numElems = 0;

    // Compute the number of elements.
    if (SubType *subtype = dyn_cast<SubType>(indexType))
        indexType = subtype->getTypeOf();

    if (IntegerType *intTy = dyn_cast<IntegerType>(indexType)) {
        llvm::APInt lower(intTy->getLowerBound());
        llvm::APInt upper(intTy->getUpperBound());

        // FIXME: Empty ranges are possible.  Should we accept them as indices?
        assert(lower.slt(upper) && "Invalid range for array index!");

        // Compute 'upper - lower + 1' to determine the number of elements in
        // this type.
        unsigned width = intTy->getBitWidth() + 1;
        lower.sext(width);
        upper.sext(width);
        llvm::APInt range(upper);
        range -= lower;
        range++;

        // If the range cannot fit in 64 bits, do not construct the type.
        assert(range.getActiveBits() <= 64 &&
               "Index type too wide for array type!");

        numElems = range.getZExtValue();
    }
    else if (EnumerationType *enumTy = dyn_cast<EnumerationType>(indexType)) {
        numElems = enumTy->getNumElements();
    }

    const llvm::Type *elementTy = lowerType(type->getComponentType());
    return llvm::ArrayType::get(elementTy, numElems);
}

const llvm::ArrayType *CodeGenTypes::lowerArraySubType(const ArraySubType *type)
{
    assert(type->getRank() == 1 &&
           "Cannot codegen multidimensional arrays yet!");

    // If the array is unconstrained, emit a variable length array type, which
    // in LLVM is represented as an array with zero elements.
    if (!type->isConstrained()) {
        const llvm::Type *elementTy = lowerType(type->getComponentType());
        return llvm::ArrayType::get(elementTy, 0);
    }

    SubType *constraint = type->getIndexConstraint(0);
    assert(constraint->isDiscreteType() && "Bad type for array index!");

    // Coumpute the bounds for the index.  If the index is itself range
    // constrained use the bounds of the range, otherwise use the bounds of the
    // base type.
    llvm::APInt lowerBound;
    llvm::APInt upperBound;
    if (RangeConstraint *range =
        dyn_cast<RangeConstraint>(constraint->getConstraint())) {
        lowerBound = range->getLowerBound();
        upperBound = range->getUpperBound();
    }
    else if (IntegerType *intTy = constraint->getAsIntegerType()) {
        lowerBound = intTy->getLowerBound();
        upperBound = intTy->getUpperBound();
    }
    else if (EnumerationType *enumTy = constraint->getAsEnumType()) {
        lowerBound = 0;
        upperBound = enumTy->getNumElements() - 1;
    }

    uint64_t numElems = getArrayWidth(lowerBound, upperBound);

    const llvm::Type *elementTy = lowerType(type->getComponentType());
    return llvm::ArrayType::get(elementTy, numElems);
}

uint64_t CodeGenTypes::getArrayWidth(const llvm::APInt &low,
                                     const llvm::APInt &high)
{
    llvm::APInt lower(low);
    llvm::APInt upper(high);

    // FIXME: Handle null ranges.
    assert(lower.slt(upper) && "Cannot codegen null ranges!");

    // Compute 'upper - lower + 1' to determine the number of elements in
    // this type.
    unsigned width = std::max(lower.getBitWidth(), upper.getBitWidth()) + 1;
    lower.sext(width);
    upper.sext(width);
    llvm::APInt range(upper);
    range -= lower;
    range++;

    // Ensure the range can fit in 64 bits.
    assert(range.getActiveBits() <= 64 &&
           "Index type too wide for array type!");

    return range.getZExtValue();
}

const llvm::StructType *
CodeGenTypes::lowerArrayBounds(const ArraySubType *arrTy)
{
    // FIXME: The width of the bounds is target dependent -- i32 for 32 bit
    // machines and i64 for 64 bit machines.
    std::vector<const llvm::Type*> elts;
    ArrayType *baseTy = arrTy->getTypeOf();

    for (unsigned i = 0; i < baseTy->getRank(); ++i) {
        const llvm::Type *boundTy = lowerType(baseTy->getIndexType(i));
        elts.push_back(boundTy);
        elts.push_back(boundTy);
    }

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
