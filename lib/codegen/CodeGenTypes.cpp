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
    switch (type->getKind()) {

    default:
        assert(false && "Cannot lower the given Type!");
        return 0;

    case Ast::AST_DomainType:
        return lowerDomainType(cast<DomainType>(type));

    case Ast::AST_CarrierType:
        return lowerCarrierType(cast<CarrierType>(type));

    case Ast::AST_EnumerationType:
        return lowerEnumType(cast<EnumerationType>(type));

    case Ast::AST_TypedefType:
        return lowerTypedefType(cast<TypedefType>(type));

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
    return lowerType(type->getRepresentationType());
}

const llvm::IntegerType *
CodeGenTypes::lowerEnumType(const EnumerationType *type)
{
    // Enumeration types are lowered to an integer type with sufficient capacity
    // to hold each element of the enumeration.
    const EnumerationDecl *decl = type->getEnumerationDecl();
    unsigned numBits = llvm::Log2_32_Ceil(decl->getNumLiterals());
    return getTypeForWidth(numBits);
}

const llvm::FunctionType *
CodeGenTypes::lowerSubroutine(const SubroutineDecl *decl)
{
    std::vector<const llvm::Type*> args;
    const llvm::FunctionType *result;

    // Emit the implicit first "%" argument.
    args.push_back(CG.getRuntime().getType<CommaRT::CRT_DomainInstance>());

    for (unsigned i = 0; i < decl->getArity(); ++i) {
        const llvm::Type *argTy = lowerType(decl->getParamType(i));

        // If the argument mode is "out" or "in out", make the argument a
        // pointer-to type.
        PM::ParameterMode mode = decl->getParamMode(i);
        if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
            argTy = CG.getPointerType(argTy);
        args.push_back(argTy);
    }

    if (const FunctionDecl *fdecl = dyn_cast<FunctionDecl>(decl)) {
        const llvm::Type *retTy = lowerType(fdecl->getReturnType());
        result = llvm::FunctionType::get(retTy, args, false);
    }
    else
        result = llvm::FunctionType::get(CG.getVoidTy(), args, false);

    return result;
}

const llvm::Type *CodeGenTypes::lowerTypedefType(const TypedefType *type)
{
    return lowerType(type->getBaseType());
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
    if (TypedefType *defTy = dyn_cast<TypedefType>(indexType))
        indexType = defTy->getBaseType();

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
        const EnumerationDecl *enumDecl = enumTy->getEnumerationDecl();
        numElems = enumDecl->getNumLiterals();
    }

    const llvm::Type *elementTy = lowerType(type->getComponentType());
    return llvm::ArrayType::get(elementTy, numElems);
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
        assert(false && "Enumeration type too large to codegen!");
        return 0;
    }
}
