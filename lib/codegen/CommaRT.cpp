//===-- codegen/CommaRT.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "Frame.h"
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
    : CG(CG)
{
    generateRuntimeTypes();
    generateRuntimeFunctions();
}

void CommaRT::generateRuntimeTypes() { }

void CommaRT::generateRuntimeFunctions()
{
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
    define_alloc();
}

void CommaRT::defineEHPersonality()
{
    // This function is never called directly, as does not need a full
    // definition.
    const llvm::Type *retTy = CG.getInt32Ty();
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, false);
    EHPersonalityFn = CG.makeFunction(fnTy, "_comma_eh_personality");
}

void CommaRT::defineUnhandledException()
{
    // This function takes a simple i8* object denoting the uncaught exception
    // object.
    const llvm::Type *retTy = CG.getVoidTy();

    std::vector<const llvm::Type *> args;
    args.push_back(CG.getInt8PtrTy());
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, false);

    unhandledExceptionFn = CG.makeFunction(fnTy, "_comma_unhandled_exception");
    unhandledExceptionFn->setDoesNotReturn();
}

void CommaRT::defineRaiseException()
{
    // _comma_raise_exception takes an i8* holding the exinfo object, an i8*
    // designating the file name, an i32 designating the line number, and an i8*
    // denoting the message.
    const llvm::Type *retTy = CG.getVoidTy();

    std::vector<const llvm::Type *> args;
    args.push_back(CG.getInt8PtrTy());
    args.push_back(CG.getInt8PtrTy());
    args.push_back(CG.getInt32Ty());
    args.push_back(CG.getInt8PtrTy());
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, false);

    raiseStaticExceptionFn = CG.makeFunction(fnTy, "_comma_raise_exception");
    raiseStaticExceptionFn->setDoesNotReturn();

    // _comma_raise_nexception is as _comma_raise_exception except that an
    // additional i32 is given yielding the message length.
    args.push_back(CG.getInt32Ty());
    fnTy = llvm::FunctionType::get(retTy, args, false);

    raiseUserExceptionFn = CG.makeFunction(fnTy, "_comma_raise_nexception");
    raiseUserExceptionFn->setDoesNotReturn();

    // _comma_reraise_exception take an i8* pointing to the exception object.
    args.clear();
    args.push_back(CG.getInt8PtrTy());
    fnTy = llvm::FunctionType::get(retTy, args, false);

    reraiseExceptionFn = CG.makeFunction(fnTy, "_comma_reraise_exception");
    reraiseExceptionFn->setDoesNotReturn();
}

void CommaRT::defineExinfos()
{
    // Comma's predefined exception info objects are just external i8*'s with
    // definitions in libruntime (see lib/runtime/crt_exceptions.c).
    const llvm::Type *exinfoTy = CG.getInt8Ty();
    llvm::Module *M = CG.getModule();

    theProgramErrorExinfo =
        new llvm::GlobalVariable(*M, exinfoTy, true,
                                 llvm::GlobalValue::ExternalLinkage,
                                 0, "_comma_exinfo_program_error");
    theConstraintErrorExinfo =
        new llvm::GlobalVariable(*M, exinfoTy, true,
                                 llvm::GlobalValue::ExternalLinkage,
                                 0, "_comma_exinfo_constraint_error");
    theAssertErrorExinfo =
        new llvm::GlobalVariable(*M, exinfoTy, true,
                                 llvm::GlobalValue::ExternalLinkage,
                                 0, "_comma_exinfo_assertion_error");
}

void CommaRT::define_pow_i32_i32()
{
    // int32_t _comma_pow_i32_i32(int32_t, int32_t);
    const llvm::Type *i32Ty = CG.getInt32Ty();

    std::vector<const llvm::Type*> args;
    args.push_back(i32Ty);
    args.push_back(i32Ty);
    llvm::FunctionType *fnTy = llvm::FunctionType::get(i32Ty, args, false);
    pow_i32_i32_Fn = CG.makeFunction(fnTy, "_comma_pow_i32_i32");
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
    pow_i64_i32_Fn = CG.makeFunction(fnTy, "_comma_pow_i64_i32");
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

void CommaRT::define_alloc()
{
    // comma_alloc takes a size argument of the C type uintptr_t and an
    // alignment of type i32.  Returns a generic i8*.
    std::vector<const llvm::Type*>args;
    args.push_back(CG.getIntPtrTy());
    args.push_back(CG.getInt32Ty());
    llvm::FunctionType *fnTy =
        llvm::FunctionType::get(CG.getInt8PtrTy(), args, false);
    alloc_Fn = CG.makeFunction(fnTy, "_comma_alloc");
}

void CommaRT::unhandledException(llvm::IRBuilder<> &builder,
                                 llvm::Value *exception) const
{
    builder.CreateCall(unhandledExceptionFn, exception);
    builder.CreateUnreachable();
}

llvm::Constant *
CommaRT::checkAndConvertMessage(llvm::GlobalVariable *message) const
{
    llvm::Constant *result;
    if (message) {
        // FIXME: Wrap the following sanity check in a DEBUG macro.
        if (llvm::Constant *init = message->getInitializer()) {
            llvm::ConstantArray *arr = cast<llvm::ConstantArray>(init);
            assert(arr->isCString() && "Message is not null terminated!");
            arr = 0;            // Inhibit unused variable warning.
        }
        result = CG.getPointerCast(message, CG.getInt8PtrTy());
    }
    else
        result = llvm::ConstantPointerNull::get(CG.getInt8PtrTy());
    return result;
}

void CommaRT::raise(Frame *frame, const ExceptionDecl *exception,
                    llvm::Value *fileName, llvm::Value *lineNum,
                    llvm::GlobalVariable *message)
{
    llvm::Value *exinfo = registerException(exception);
    raiseExinfo(frame, exinfo, fileName, lineNum, message);
}

void CommaRT::raise(Frame *frame, const ExceptionDecl *exception,
                    llvm::Value *fileName, llvm::Value *lineNum,
                    llvm::Value *message, llvm::Value *length)
{
    llvm::Value *exinfo = registerException(exception);
    raiseExinfo(frame, exinfo, fileName, lineNum, message, length);
}

void CommaRT::raiseExinfo(Frame *frame, llvm::Value *exinfo,
                          llvm::Value *fileName, llvm::Value *lineNum,
                          llvm::GlobalVariable *message) const
{
    llvm::IRBuilder<> &builder = frame->getIRBuilder();
    llvm::Constant *msgPtr = checkAndConvertMessage(message);
    if (llvm::BasicBlock *lpad = frame->getLandingPad()) {
        llvm::BasicBlock *norm = frame->makeBasicBlock("invoke.normal");
        llvm::Value *args[4] = { exinfo, fileName, lineNum, msgPtr };
        builder.CreateInvoke(raiseStaticExceptionFn, norm, lpad, args, args+4);
        builder.SetInsertPoint(norm);
    }
    else
        builder.CreateCall4(raiseStaticExceptionFn, exinfo,
                            fileName, lineNum, msgPtr);
    builder.CreateUnreachable();
}

void CommaRT::raiseExinfo(Frame *frame, llvm::Value *exinfo,
                          llvm::Value *fileName, llvm::Value *lineNum,
                          llvm::Value *message, llvm::Value *length) const
{
    llvm::IRBuilder<> &builder = frame->getIRBuilder();

    if (message)
        message = builder.CreatePointerCast(message, CG.getInt8PtrTy());
    else {
        message = llvm::ConstantPointerNull::get(CG.getInt8PtrTy());
        length = llvm::ConstantInt::get(CG.getInt32Ty(), 0);
    }

    llvm::Value *args[5] = { exinfo, fileName, lineNum, message, length };

    if (llvm::BasicBlock *lpad = frame->getLandingPad()) {
        llvm::BasicBlock *norm = frame->makeBasicBlock("invoke.normal");
        builder.CreateInvoke(raiseUserExceptionFn, norm, lpad, args, args+5);
        builder.SetInsertPoint(norm);
    }
    else
        builder.CreateCall(raiseUserExceptionFn, args, args+5);
    builder.CreateUnreachable();
}

void CommaRT::reraise(Frame *frame, llvm::Value *exception)
{
    llvm::IRBuilder<> &builder = frame->getIRBuilder();

    if (llvm::BasicBlock *lpad = frame->getLandingPad()) {
        llvm::BasicBlock *norm = frame->makeBasicBlock("invoke.normal");
        llvm::Value *args[1] = { exception };
        builder.CreateInvoke(reraiseExceptionFn, norm, lpad, args, args+1);
        builder.SetInsertPoint(norm);
    }
    else
        builder.CreateCall(reraiseExceptionFn, exception);
    builder.CreateUnreachable();
}

void CommaRT::raiseProgramError(Frame *frame,
                                llvm::Value *fileName, llvm::Value *lineNum,
                                llvm::GlobalVariable *message) const
{
    raiseExinfo(frame, theProgramErrorExinfo, fileName, lineNum, message);
}

void CommaRT::raiseConstraintError(Frame *frame,
                                   llvm::Value *fileName, llvm::Value *lineNum,
                                   llvm::GlobalVariable *message) const
{
    raiseExinfo(frame, theConstraintErrorExinfo, fileName, lineNum, message);
}

void
CommaRT::raiseAssertionError(Frame *frame,
                             llvm::Value *fileName, llvm::Value *lineNum,
                             llvm::Value *message, llvm::Value *length) const
{
    raiseExinfo(frame, theAssertErrorExinfo,
                fileName, lineNum, message, length);
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
            llvm::Constant *init = genExinfoInitializer(except);
            const llvm::Type *exinfoTy = init->getType();
            entry = new llvm::GlobalVariable(*CG.getModule(), exinfoTy, true,
                                             llvm::GlobalValue::ExternalLinkage,
                                             init, mangle::getLinkName(except));
            exinfo = CG.getPointerCast(entry, CG.getInt8PtrTy());
        }
        else
            exinfo = entry;
    }

    case ExceptionDecl::Program_Error:
        exinfo = theProgramErrorExinfo;
        break;

    case ExceptionDecl::Constraint_Error:
        exinfo = theConstraintErrorExinfo;
        break;

    case ExceptionDecl::Assertion_Error:
        exinfo = theAssertErrorExinfo;
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

llvm::Value *CommaRT::comma_alloc(llvm::IRBuilder<> &builder,
                                  uint64_t size, unsigned alignment) const
{
    const llvm::FunctionType *allocTy = alloc_Fn->getFunctionType();
    const llvm::Type *sizeTy = allocTy->getParamType(0);
    const llvm::Type *alignTy = allocTy->getParamType(1);

    llvm::Value *sizeVal = llvm::ConstantInt::get(sizeTy, size);
    llvm::Value *alignVal = llvm::ConstantInt::get(alignTy, alignment);

    // FIXME:  Check for null and raise PROGRAM_ERROR when needed.
    return builder.CreateCall2(alloc_Fn, sizeVal, alignVal);
}

llvm::Value *CommaRT::comma_alloc(llvm::IRBuilder<> &builder,
                                  llvm::Value *size, unsigned alignment) const
{
    const llvm::FunctionType *allocTy = alloc_Fn->getFunctionType();
    const llvm::Type *sizeTy = allocTy->getParamType(0);
    const llvm::Type *alignTy = allocTy->getParamType(1);

    if (size->getType() != sizeTy)
        size = builder.CreateZExt(size, sizeTy);

    llvm::Value *align = llvm::ConstantInt::get(alignTy, alignment);

    // FIXME:  Check for null and raise PROGRAM_ERROR when needed.
    return builder.CreateCall2(alloc_Fn, size, align);
}

llvm::Value *CommaRT::comma_alloc(llvm::IRBuilder<> &builder,
                                  const llvm::Type *type, uint64_t nmemb) const
{
    llvm::Constant *size = llvm::ConstantExpr::getSizeOf(type);
    llvm::Constant *numElt = llvm::ConstantInt::get(size->getType(), nmemb);
    llvm::Value *ptr;

    size = llvm::ConstantExpr::getMul(size, numElt);
    size = llvm::ConstantExpr::getTrunc(size, CG.getInt32Ty());
    ptr = comma_alloc(builder, size, 0); // FIXME: Implement alignment.
    return builder.CreatePointerCast(ptr, type->getPointerTo());
}

llvm::Constant *CommaRT::genExinfoInitializer(const ExceptionDecl *exception)
{
    // FIXME: exinfo's are just global null terminated strings.  We should be
    // doing a bit of pretty printing here to get a readable qualified name for
    // the exception.
    llvm::LLVMContext &ctx = CG.getLLVMContext();
    return llvm::ConstantArray::get(ctx, exception->getString(), true);
}

