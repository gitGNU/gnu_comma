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

    case Ast::AST_FunctionType:
    case Ast::AST_ProcedureType:
        return lowerSubroutineType(cast<SubroutineType>(type));
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
        RV.first = instance->getActualParameter(i);
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

    if (type->denotesPercent()) {
        const Domoid *domoid = cast<Domoid>(type->getDeclaration());
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
CodeGenTypes::lowerSubroutineType(const SubroutineType *type)
{
    std::vector<const llvm::Type*> args;
    const llvm::FunctionType *result;

    // Emit the implicit first "%" argument.
    args.push_back(CG.getRuntime().getType<CommaRT::CRT_DomainInstance>());

    for (unsigned i = 0; i < type->getArity(); ++i) {
        const llvm::Type *argTy = lowerType(type->getArgType(i));

        // If the argument mode is "out" or "in out", make the argument a
        // pointer-to type.
        PM::ParameterMode mode = type->getParameterMode(i);
        if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
            argTy = CG.getPointerType(argTy);
        args.push_back(argTy);
    }

    if (const FunctionType *ftype = dyn_cast<FunctionType>(type)) {
        const llvm::Type *retTy = lowerType(ftype->getReturnType());
        result = llvm::FunctionType::get(retTy, args, false);
    }
    else
        result = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);

    return result;
}

const llvm::IntegerType *CodeGenTypes::lowerTypedefType(const TypedefType *type)
{
    // Currently, all TypedefType's are Integer types.
    const IntegerType *baseType = cast<IntegerType>(type->getBaseType());
    return lowerIntegerType(baseType);
}

const llvm::IntegerType *CodeGenTypes::lowerIntegerType(const IntegerType *type)
{
    return getTypeForWidth(type->getBitWidth());
}

const llvm::IntegerType *CodeGenTypes::getTypeForWidth(unsigned numBits)
{
    // Promote the bit width to a power of two.
    if (numBits <= 1)
        return llvm::Type::Int1Ty;
    if (numBits <= 8)
        return llvm::Type::Int8Ty;
    else if (numBits <= 16)
        return llvm::Type::Int16Ty;
    else if (numBits <= 32)
        return llvm::Type::Int32Ty;
    else if (numBits <= 64)
        return llvm::Type::Int64Ty;
    else {
        // FIXME: This should be a fatal error, not an assert.
        assert(false && "Enumeration type too large to codegen!");
        return 0;
    }
}
