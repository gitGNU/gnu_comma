//===-- codegen/CommaRT.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenCapsule.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "DomainInfo.h"
#include "DomainInstance.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/Mangle.h"

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
    defineExinfos();
    define_pow_i32_i32();
    define_pow_i64_i32();
    define_vstack();
    define_vstack_alloc();
    define_vstack_push();
    define_vstack_pop();
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
    // This function takes an i8* holding the exinfo object and an i8* denoting
    // the message.
    const llvm::Type *retTy = CG.getVoidTy();

    std::vector<const llvm::Type *> args;
    args.push_back(CG.getInt8PtrTy());
    args.push_back(CG.getInt8PtrTy());
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, false);

    raiseExceptionFn = CG.makeFunction(fnTy, "_comma_raise_exception");
    raiseExceptionFn->setDoesNotReturn();
}

void CommaRT::defineExinfos()
{
    // Comma's predefined exception info objects are just external i8*'s with
    // definitions in libruntime (see lib/runtime/crt_exceptions.c).
    const llvm::Type *exinfoTy = CG.getInt8PtrTy();

    theProgramErrorExinfo =
        new llvm::GlobalVariable(*CG.getModule(), exinfoTy, true,
                                 llvm::GlobalValue::ExternalLinkage,
                                 0, "_comma_exinfo_program_error");
    theConstraintErrorExinfo =
        new llvm::GlobalVariable(*CG.getModule(), exinfoTy, true,
                                 llvm::GlobalValue::ExternalLinkage,
                                 0, "_comma_exinfo_constraint_error");
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

void CommaRT::define_vstack_alloc()
{
    // void _comma_vstack_alloc(int32_t);
    std::vector<const llvm::Type*> args;
    args.push_back(CG.getInt32Ty());
    llvm::FunctionType *fnTy =
        llvm::FunctionType::get(CG.getVoidTy(), args, false);
    vstack_alloc_Fn = CG.makeFunction(fnTy, "_comma_vstack_alloc");
    vstack_alloc_Fn->setDoesNotThrow();
}

void CommaRT::define_vstack_push()
{
    // void _comma_vstack_push(char *, int32_t);
    std::vector<const llvm::Type*> args;
    args.push_back(CG.getInt8PtrTy());
    args.push_back(CG.getInt32Ty());
    llvm::FunctionType *fnTy =
        llvm::FunctionType::get(CG.getVoidTy(), args, false);
    vstack_push_Fn = CG.makeFunction(fnTy, "_comma_vstack_push");
    vstack_push_Fn->setDoesNotThrow();
}

void CommaRT::define_vstack_pop()
{
    // void _comma_vstack_pop();
    std::vector<const llvm::Type*> args;
    llvm::FunctionType *fnTy =
        llvm::FunctionType::get(CG.getVoidTy(), args, false);
    vstack_pop_Fn = CG.makeFunction(fnTy, "_comma_vstack_pop");
    vstack_pop_Fn->setDoesNotThrow();
}

void CommaRT::define_vstack()
{
    vstack_Var =
        new llvm::GlobalVariable(*CG.getModule(), CG.getInt8PtrTy(), true,
                                 llvm::GlobalValue::ExternalLinkage,
                                 0, "_comma_vstack");
}

llvm::GlobalVariable *CommaRT::registerCapsule(Domoid *domoid)
{
    return DInfo->emit(domoid);
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
                    const ExceptionDecl *exception,
                    llvm::GlobalVariable *message)
{
    llvm::Value *exinfo = builder.CreateLoad(registerException(exception));
    llvm::Constant *msgPtr = CG.getPointerCast(message, CG.getInt8PtrTy());
    builder.CreateCall2(raiseExceptionFn, exinfo, msgPtr);
    builder.CreateUnreachable();
}

void CommaRT::raiseProgramError(llvm::IRBuilder<> &builder,
                                llvm::GlobalVariable *message) const
{
    llvm::Value *exinfo = builder.CreateLoad(theProgramErrorExinfo);
    llvm::Constant *msgPtr = CG.getPointerCast(message, CG.getInt8PtrTy());
    builder.CreateCall2(raiseExceptionFn, exinfo, msgPtr);
    builder.CreateUnreachable();
}

void CommaRT::raiseConstraintError(llvm::IRBuilder<> &builder,
                                   llvm::GlobalVariable *message) const
{
    llvm::Value *exinfo = builder.CreateLoad(theConstraintErrorExinfo);
    llvm::Constant *msgPtr = CG.getPointerCast(message, CG.getInt8PtrTy());
    builder.CreateCall2(raiseExceptionFn, exinfo, msgPtr);
    builder.CreateUnreachable();
}

llvm::Constant *CommaRT::getEHPersonality() const
{
    return CG.getPointerCast(EHPersonalityFn, CG.getInt8PtrTy());
}

llvm::Constant *CommaRT::registerException(const ExceptionDecl *except)
{
    llvm::Constant *exinfo = 0;

    switch (except->getID()) {

    case ExceptionDecl::User: {
        llvm::GlobalVariable *&entry = registeredExceptions[except];
        if (!entry) {
            const llvm::Type *exinfoTy = CG.getInt8PtrTy();
            llvm::Constant *init = genExinfoInitializer(except);
            entry = new llvm::GlobalVariable(*CG.getModule(), exinfoTy, true,
                                             llvm::GlobalValue::ExternalLinkage,
                                             init, mangle::getLinkName(except));
        }
        exinfo = entry;
    }

    case ExceptionDecl::Program_Error:
        exinfo = theProgramErrorExinfo;
        break;

    case ExceptionDecl::Constraint_Error:
        exinfo = theConstraintErrorExinfo;
        break;
    }
    return exinfo;
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

void CommaRT::vstack_alloc(llvm::IRBuilder<> &builder, llvm::Value *size) const
{
    builder.CreateCall(vstack_alloc_Fn, size);
}

void CommaRT::vstack_push(llvm::IRBuilder<> &builder,
                          llvm::Value *data, llvm::Value *size) const
{
    data = builder.CreatePointerCast(data, CG.getInt8PtrTy());
    builder.CreateCall2(vstack_push_Fn, data, size);
}

void CommaRT::vstack_pop(llvm::IRBuilder<> &builder) const
{
    builder.CreateCall(vstack_pop_Fn);
}

llvm::Value *CommaRT::vstack(llvm::IRBuilder<> &builder,
                             const llvm::Type *type) const
{
    // Always perform a volatile load of the vstack pointer as it is most often
    // accessed between calls to _comma_vstack_pop.
    const llvm::PointerType *ptrTy = cast<llvm::PointerType>(type);
    llvm::Value *stack_data = builder.CreateLoad(vstack_Var, true);
    return builder.CreatePointerCast(stack_data, ptrTy);
}

llvm::Value *CommaRT::getLocalCapsule(llvm::IRBuilder<> &builder,
                                      llvm::Value *percent, unsigned ID) const
{
    return DInstance->loadLocalInstance(builder, percent, ID);
}

/// Returns the formal parameter with the given index.
llvm::Value *CommaRT::getCapsuleParameter(llvm::IRBuilder<> &builder,
                                          llvm::Value *instance,
                                          unsigned index) const
{
    return DInstance->loadParam(builder, instance, index);
}

llvm::Constant *CommaRT::genExinfoInitializer(const ExceptionDecl *exception)
{
    // FIXME: exinfo's are just global null terminated strings.  We should be
    // doing a bit of pretty printing here to get a readable qualified name for
    // the exception.
    llvm::LLVMContext &ctx = CG.getLLVMContext();
    return llvm::ConstantArray::get(ctx, exception->getString(), true);
}

