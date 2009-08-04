//===-- codegen/DomainInstance.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DomainInstance.h"
#include "comma/codegen/CommaRT.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DomainInstance::DomainInstance(CommaRT &CRT)
    : CRT(CRT),
      CG(CRT.getCodeGen()),
      TD(CG.getTargetData()),
      theType(llvm::OpaqueType::get()) { }

void DomainInstance::init()
{
    std::vector<const llvm::Type*> members;

    const llvm::PointerType *InfoPtrTy;
    const llvm::PointerType *ViewPtrTy;
    const llvm::Type *ViewTy;
    const llvm::PointerType *InstancePtrTy;

    InfoPtrTy = CRT.getType<CommaRT::CRT_DomainInfo>();
    ViewPtrTy = CRT.getType<CommaRT::CRT_DomainView>();
    ViewTy = ViewPtrTy->getElementType();
    InstancePtrTy = llvm::PointerType::getUnqual(theType.get());

    members.push_back(InfoPtrTy);
    members.push_back(theType.get());
    members.push_back(CG.getPointerType(ViewPtrTy));
    members.push_back(CG.getPointerType(ViewTy));
    members.push_back(CG.getPointerType(InstancePtrTy));

    llvm::StructType *InstanceTy = llvm::StructType::get(members);
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

    llvm::Value *index = llvm::ConstantInt::get(llvm::Type::Int32Ty, paramIdx);

    llvm::Value *paramVec = loadParamVec(builder, instance);
    llvm::Value *viewAddr = builder.CreateGEP(paramVec, index);
    return builder.CreateLoad(viewAddr);
}

/// Loads a pointer to the first view in the supplied domain_instance's view
/// vector.
llvm::Value *DomainInstance::loadViewVec(llvm::IRBuilder<> &builder,
                                         llvm::Value *instance) const
{
    assert(instance->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *vecAddr = builder.CreateStructGEP(instance, Views);
    return builder.CreateLoad(vecAddr);
}

/// Loads the domain_view of the supplied domain_instance object corresponding
/// to the signature with the given index.
llvm::Value *DomainInstance::loadView(llvm::IRBuilder<> &builder,
                                      llvm::Value *instance,
                                      llvm::Value *sigIndex) const
{
    assert(instance->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *viewArr = loadViewVec(builder, instance);
    return builder.CreateGEP(viewArr, sigIndex);
}


/// Loads the domain_view of the supplied domain_instance object corresponding
/// to the signature with the given index.
llvm::Value *DomainInstance::loadView(llvm::IRBuilder<> &builder,
                                      llvm::Value *instance,
                                      unsigned sigIndex) const
{
    llvm::Value *index = llvm::ConstantInt::get(llvm::Type::Int32Ty, sigIndex);
    return loadView(builder, instance, index);
}
