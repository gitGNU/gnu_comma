//===-- codegen/CommaRT.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DomainInfo.h"
#include "DomainInstance.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/ADT/IndexedMap.h"
#include "llvm/Module.h"
#include "llvm/Value.h"
#include "llvm/Support/IRBuilder.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CommaRT::CommaRT(CodeGen &CG)
    : CG(CG),
      ITableName("_comma_itable_t"),
      DomainCtorName("_comma_domain_ctor_t"),

      DInfo(0),
      DomainInfoPtrTy(0),

      DInstance(0),
      DomainInstancePtrTy(0),

      ITablePtrTy(getITablePtrTy()),
      DomainCtorPtrTy(0),

      GetDomainName("_comma_get_domain")
{
    DInfo = new DomainInfo(*this);
    DomainInfoPtrTy = DInfo->getPointerTypeTo();

    DInstance = new DomainInstance(*this);
    DomainInstancePtrTy = DInstance->getPointerTypeTo();

    DomainCtorPtrTy = DInfo->getCtorPtrType();

    DInfo->init();
    DInstance->init();

    generateRuntimeTypes();
    generateRuntimeFunctions();
}

CommaRT::~CommaRT()
{
    delete DInfo;
    delete DInstance;
}

const std::string &CommaRT::getTypeName(TypeId id) const
{
    switch (id) {
    default:
        assert(false && "Invalid type id!");
        return InvalidName;
    case CRT_ITable:
        return ITableName;
    case CRT_DomainInfo:
        return DInfo->getTypeName();
    case CRT_DomainInstance:
        return DInstance->getTypeName();
    case CRT_DomainCtor:
        return DomainCtorName;
    }
}

void CommaRT::generateRuntimeTypes()
{
    // Define the types within the Module.
    llvm::Module *M = CG.getModule();
    M->addTypeName(getTypeName(CRT_DomainInfo),     getType<CRT_DomainInfo>());
    M->addTypeName(getTypeName(CRT_DomainInstance), getType<CRT_DomainInstance>());
}

const llvm::PointerType *CommaRT::getDomainCtorPtrTy()
{
    std::vector<const llvm::Type*> args;

    args.push_back(DomainInstancePtrTy);

    const llvm::Type *ctorTy = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);
    return CG.getPointerType(ctorTy);
}

const llvm::PointerType *CommaRT::getITablePtrTy()
{
    return CG.getPointerType(llvm::Type::Int8Ty);
}

void CommaRT::generateRuntimeFunctions()
{
    defineGetDomain();
}

// Builds a declaration in LLVM IR for the get_domain runtime function.
void CommaRT::defineGetDomain()
{
    const llvm::Type *retTy = getType<CRT_DomainInstance>();
    std::vector<const llvm::Type *> args;

    args.push_back(getType<CRT_DomainInfo>());

    // get_domain takes a pointer to a domain_instance_t as first argument, and
    // then a variable number of domain_instance_t args corresponding to the
    // parameters.
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, true);

    getDomainFn = CG.makeFunction(fnTy, GetDomainName);
}

llvm::GlobalVariable *CommaRT::registerCapsule(CodeGenCapsule &CGC)
{
    return DInfo->generateInstance(CGC);
}

llvm::Value *CommaRT::getDomain(llvm::IRBuilder<> &builder,
                                llvm::GlobalValue *capsuleInfo) const
{
    return builder.CreateCall(getDomainFn, capsuleInfo);
}

llvm::Value *CommaRT::getDomain(llvm::IRBuilder<> &builder,
                                std::vector<llvm::Value *> &args) const
{
    assert(args.front()->getType() == getType<CRT_DomainInfo>()
           && "First argument is not a domain_info_t!");
    return builder.CreateCall(getDomainFn, args.begin(), args.end());
}

llvm::Value *CommaRT::getLocalCapsule(llvm::IRBuilder<> &builder,
                                      llvm::Value *percent, unsigned ID) const
{
    return DInstance->loadLocalInstance(builder, percent, ID - 1);
}

/// Returns the formal parameter with the given index.
llvm::Value *CommaRT::getCapsuleParameter(llvm::IRBuilder<> &builder,
                                          llvm::Value *instance,
                                          unsigned index) const
{
    return DInstance->loadParam(builder, instance, index);
}
