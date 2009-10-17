//===-- codegen/CommaRT.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenCapsule.h"
#include "DomainInfo.h"
#include "DomainInstance.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
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

      GetDomainName("_comma_get_domain"),
      AssertFailName("_comma_assert_fail"),
      EHPersonalityName("_comma_eh_personality"),
      UnhandledExceptionName("_comma_unhandled_exception"),
      RaiseExceptionName("_comma_raise_exception"),
      pow_i32_i32_Name("_comma_pow_i32_i32"),
      pow_i64_i32_Name("_comma_pow_i64_i32")
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

    const llvm::Type *ctorTy
        = llvm::FunctionType::get(CG.getVoidTy(), args, false);
    return CG.getPointerType(ctorTy);
}

const llvm::PointerType *CommaRT::getITablePtrTy()
{
    return CG.getInt8PtrTy();
}

void CommaRT::generateRuntimeFunctions()
{
    defineGetDomain();
    defineAssertFail();
    defineEHPersonality();
    defineUnhandledException();
    defineRaiseException();
    define_pow_i32_i32();
    define_pow_i64_i32();
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

void CommaRT::defineAssertFail()
{
    const llvm::Type *retTy = CG.getVoidTy();

    std::vector<const llvm::Type *> args;
    args.push_back(CG.getInt8PtrTy());

    // _comma_assert_fail takes a pointer to a C string as argument, and does
    // not return.
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, false);

    assertFailFn = CG.makeFunction(fnTy, AssertFailName);
    assertFailFn->setDoesNotReturn();
}

void CommaRT::defineEHPersonality()
{
    // This function is never called directly, as does not need a full
    // definition.
    const llvm::Type *retTy = CG.getInt32Ty();
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, false);
    EHPersonalityFn = CG.makeFunction(fnTy, EHPersonalityName);
}

void CommaRT::defineUnhandledException()
{
    // This function takes a simple i8* object denoting the uncaught exception
    // object.
    const llvm::Type *retTy = CG.getVoidTy();

    std::vector<const llvm::Type *> args;
    args.push_back(CG.getInt8PtrTy());
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, false);

    unhandledExceptionFn = CG.makeFunction(fnTy, UnhandledExceptionName);
    unhandledExceptionFn->setDoesNotReturn();
}

void CommaRT::defineRaiseException()
{
    // This function takes an i8* denoting a message to pack into the thrown
    // exception.
    const llvm::Type *retTy = CG.getVoidTy();

    std::vector<const llvm::Type *> args;
    args.push_back(CG.getInt8PtrTy());
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, false);

    raiseExceptionFn = CG.makeFunction(fnTy, RaiseExceptionName);
    raiseExceptionFn->setDoesNotReturn();
}

void CommaRT::define_pow_i32_i32()
{
    // int32_t _comma_pow_i32_i32(int32_t, int32_t);
    const llvm::Type *i32Ty = CG.getInt32Ty();

    std::vector<const llvm::Type*> args;
    args.push_back(i32Ty);
    args.push_back(i32Ty);
    llvm::FunctionType *fnTy = llvm::FunctionType::get(i32Ty, args, false);
    pow_i32_i32_Fn = CG.makeFunction(fnTy, pow_i32_i32_Name);
}

void CommaRT::define_pow_i64_i32()
{
    // int64_t _comma_pow_i64_i32(int64_t, int32_t);
    const llvm::Type *i32Ty = CG.getInt32Ty();
    const llvm::Type *i64Ty = CG.getInt64Ty();

    std::vector<const llvm::Type*> args;
    args.push_back(i64Ty);
    args.push_back(i32Ty);
    llvm::FunctionType *fnTy = llvm::FunctionType::get(i64Ty, args, false);
    pow_i64_i32_Fn = CG.makeFunction(fnTy, pow_i64_i32_Name);
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

void CommaRT::assertFail(llvm::IRBuilder<> &builder, llvm::Value *message) const
{
    builder.CreateCall(assertFailFn, message);
    builder.CreateUnreachable();
}

void CommaRT::unhandledException(llvm::IRBuilder<> &builder,
                                 llvm::Value *exception) const
{
    builder.CreateCall(unhandledExceptionFn, exception);
    builder.CreateUnreachable();
}

void CommaRT::raise(llvm::IRBuilder<> &builder,
                    llvm::GlobalVariable *message) const
{
    llvm::Constant *msgPtr = CG.getPointerCast(message, CG.getInt8PtrTy());
    builder.CreateCall(raiseExceptionFn, msgPtr);
    builder.CreateUnreachable();
}

llvm::Constant *CommaRT::getEHPersonality() const
{
    return CG.getPointerCast(EHPersonalityFn, CG.getInt8PtrTy());
}

llvm::Value *CommaRT::pow_i32_i32(llvm::IRBuilder<> &builder,
                                  llvm::Value *x, llvm::Value *n) const
{
    return builder.CreateCall2(pow_i32_i32_Fn, x, n);
}

llvm::Value *CommaRT::pow_i64_i32(llvm::IRBuilder<> &builder,
                                  llvm::Value *x, llvm::Value *n) const
{
    return builder.CreateCall2(pow_i64_i32_Fn, x, n);
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
