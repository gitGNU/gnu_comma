//===-- codegen/DomainInstance.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CommaRT.h"
#include "DomainInstance.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DomainInstance::DomainInstance(CommaRT &CRT)
    : CRT(CRT),
      CG(CRT.getCodeGen()),
      TD(CG.getTargetData()),
      theType(CG.getOpaqueTy()) { }

void DomainInstance::init()
{
    std::vector<const llvm::Type*> members;

    const llvm::PointerType *InfoPtrTy;
    const llvm::PointerType *InstancePtrTy;

    InfoPtrTy = CRT.getType<CommaRT::CRT_DomainInfo>();
    InstancePtrTy = llvm::PointerType::getUnqual(theType.get());

    members.push_back(InfoPtrTy);
    members.push_back(theType.get());
    members.push_back(CG.getPointerType(InstancePtrTy));
    members.push_back(CG.getPointerType(InstancePtrTy));

    llvm::StructType *InstanceTy = CG.getStructTy(members);
    cast<llvm::OpaqueType>(theType.get())->refineAbstractTypeTo(InstanceTy);
}

const std::string DomainInstance::theTypeName("comma_instance_t");

const llvm::StructType *DomainInstance::getType() const
{
    return cast<llvm::StructType>(theType.get());
}

const llvm::PointerType *DomainInstance::getPointerTypeTo() const
{
    return llvm::PointerType::getUnqual(theType.get());
}

/// Loads the domain_info associated with the given domain_instance.
llvm::Value *DomainInstance::loadInfo(llvm::IRBuilder<> &builder,
                                      llvm::Value *Instance) const
{
    assert(Instance->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *InfoAddr = builder.CreateStructGEP(Instance, Info);
    return builder.CreateLoad(InfoAddr);
}

/// Loads a pointer to the first element of the parameter vector associated with
/// the given domain instance object.
llvm::Value *DomainInstance::loadParamVec(llvm::IRBuilder<> &builder,
                                          llvm::Value *instance) const
{
    assert(instance->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *paramsAddr = builder.CreateStructGEP(instance, Params);
    return builder.CreateLoad(paramsAddr);
}

/// Loads the domain_view corresponding to a formal parameter from a
/// domain_instance object, where \p index identifies which parameter is sought.
llvm::Value *DomainInstance::loadParam(llvm::IRBuilder<> &builder,
                                       llvm::Value *instance,
                                       unsigned paramIdx) const
{
    assert(instance->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *index = llvm::ConstantInt::get(CG.getInt32Ty(), paramIdx);

    llvm::Value *paramVec = loadParamVec(builder, instance);
    llvm::Value *viewAddr = builder.CreateGEP(paramVec, index);
    return builder.CreateLoad(viewAddr);
}

llvm::Value *DomainInstance::loadLocalVec(llvm::IRBuilder<> &builder,
                                          llvm::Value *instance) const
{
    llvm::Value *localVecAddr = builder.CreateStructGEP(instance, Requirements);
    return builder.CreateLoad(localVecAddr);
}

void DomainInstance::setLocalVec(llvm::IRBuilder<> &builder,
                                 llvm::Value *instance,
                                 llvm::Value *vec) const
{
    llvm::Value *dst = builder.CreateStructGEP(instance, Requirements);
    builder.CreateStore(vec, dst);
}

llvm::Value *DomainInstance::loadLocalInstance(llvm::IRBuilder<> &builder,
                                               llvm::Value *instance,
                                               unsigned ID) const
{
    assert(instance->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM value!");

    // Load the required capsule array.
    llvm::Value *elt = loadLocalVec(builder, instance);

    // Index into the required capsule array by the given ID.
    llvm::Value *index = llvm::ConstantInt::get(CG.getInt32Ty(), ID);
    elt = builder.CreateGEP(elt, index);

    // And finally load the indexed pointer, yielding the local capsule.
    return builder.CreateLoad(elt);
}
