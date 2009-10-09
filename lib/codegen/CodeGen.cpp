//===-- codegen/CodeGen.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DomainInfo.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/Support/Casting.h"

using namespace comma;

using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

CodeGen::CodeGen(llvm::Module *M, const llvm::TargetData &data,
                 AstResource &resource)
    : M(M),
      TD(data),
      Resource(resource),
      CGTypes(new CodeGenTypes(*this)),
      CRT(new CommaRT(*this)),
      CGCapsule(0) { }

CodeGen::~CodeGen()
{
    delete CGTypes;
    delete CRT;
}

CodeGenTypes &CodeGen::getTypeGenerator()
{
    return *CGTypes;
}

const CodeGenTypes &CodeGen::getTypeGenerator() const
{
    return *CGTypes;
}

CodeGenCapsule &CodeGen::getCapsuleGenerator()
{
    return *CGCapsule;
}

const CodeGenCapsule &CodeGen::getCapsuleGenerator() const
{
    return *CGCapsule;
}

void CodeGen::emitToplevelDecl(Decl *decl)
{
    if (DomainDecl *domain = dyn_cast<DomainDecl>(decl))
        CGCapsule = new CodeGenCapsule(*this, domain);
    else if (FunctorDecl *functor = dyn_cast<FunctorDecl>(decl))
        CGCapsule = new CodeGenCapsule(*this, functor);
    else
        return;

    CGCapsule->emit();

    llvm::GlobalVariable *info = CRT->registerCapsule(*CGCapsule);
    capsuleInfoTable[CGCapsule->getLinkName()] = info;
    delete CGCapsule;

    // Continuously compile the worklist so long as there exists entries
    // which need to be codegened.
    while (instancesPending())
        emitNextInstance();
}

void CodeGen::emitEntryStub(ProcedureDecl *pdecl)
{
    // Basic sanity checks on the declaration.
    assert(pdecl->getArity() == 0 && "Entry procedures must be nullary!");

    // Get the procedures declarative region. This must an instance of a
    // non-parameterized domain.
    DeclRegion *region = pdecl->getDeclRegion();
    DomainInstanceDecl *context = cast<DomainInstanceDecl>(region);
    assert(!context->isParameterized() &&
           "Cannot call entry procedures in a parameterized context!");

    // Lookup the previously codegened function for this decl.
    std::string procName = CodeGenCapsule::getLinkName(context, pdecl);
    llvm::Value *func = lookupGlobal(procName);
    assert(func && "Lookup of entry procedure failed!");

    // Build the function type for the entry stub.
    //
    // FIXME: This should be a target dependent operation.
    std::vector<const llvm::Type*> argTypes;
    argTypes.push_back(getInt32Ty());
    argTypes.push_back(getPointerType(getInt8PtrTy()));
    llvm::FunctionType *entryTy =
        llvm::FunctionType::get(getInt32Ty(), argTypes, false);

    // Get an external function to populate with the entry code.
    //
    // FIXME: The name of main function is a target dependent operation.
    llvm::Function *entry = makeFunction(entryTy, "main");

    llvm::IRBuilder<> Builder(getLLVMContext());
    llvm::BasicBlock *entryBB = makeBasicBlock("entry", entry);
    llvm::BasicBlock *landingBB = makeBasicBlock("landingpad", entry);
    llvm::BasicBlock *returnBB  = makeBasicBlock("return", entry);
    Builder.SetInsertPoint(entryBB);

    // Lookup the domain info object for the needed domain.
    llvm::GlobalValue *domainInfo = lookupCapsuleInfo(context->getDefinition());
    assert(domainInfo && "Could not find domain_info for entry function!");

    // Build the get_domain call.  Since this domain is not parameterized, only
    // the domain_info object is required as an argument.
    llvm::Value *domainInstance = CRT->getDomain(Builder, domainInfo);

    // Invoke our entry function with the generated instance as its only
    // argument.
    llvm::SmallVector<llvm::Value*, 2> args;
    args.push_back(domainInstance);
    Builder.CreateInvoke(func, returnBB, landingBB, args.begin(), args.end());

    // Switch to normal return.
    Builder.SetInsertPoint(returnBB);
    Builder.CreateRet(llvm::ConstantInt::get(getInt32Ty(), uint64_t(0)));

    // Switch to landing pad.
    Builder.SetInsertPoint(landingBB);
    llvm::Function *eh_except;
    llvm::Function *eh_select;
    llvm::Function *eh_typeid;
    llvm::Value *eh_person;
    eh_except = getLLVMIntrinsic(llvm::Intrinsic::eh_exception);
    eh_select = getLLVMIntrinsic(llvm::Intrinsic::eh_selector_i64);
    eh_typeid = getLLVMIntrinsic(llvm::Intrinsic::eh_typeid_for_i64);
    eh_person = CRT->getEHPersonality();

    // The exception object produced by a call to _comma_raise.
    llvm::Value *except = Builder.CreateCall(eh_except);

    // Build an exception selector with a null exception info entry.  This is a
    // catch-all selector.
    args.clear();
    args.push_back(except);
    args.push_back(eh_person);
    args.push_back(getNullPointer(getInt8PtrTy()));
    llvm::Value *infodx = Builder.CreateCall(eh_select, args.begin(), args.end());

    // The returned index must be positive and it must match the id for our
    // exception info object.  Generate a call to _comma_assert_fail if these
    // conditions are not met.
    llvm::Value *targetdx = Builder.CreateCall(eh_typeid,
                                               getNullPointer(getInt8PtrTy()));
    llvm::BasicBlock *catchAllBB = makeBasicBlock("catch-all", entry);
    llvm::BasicBlock *assertBB = makeBasicBlock("assert-fail", entry);
    llvm::Value *cond = Builder.CreateICmpEQ(infodx, targetdx);
    Builder.CreateCondBr(cond, catchAllBB, assertBB);

    // The catch all should certainly suceed.  Call into _comma_unhandled_exception,
    // passing it the exception object.  This function never returns.
    Builder.SetInsertPoint(catchAllBB);
    CRT->unhandledException(Builder, except);

    // Assertion case for when the catch-all failed.  This should never happen.
    Builder.SetInsertPoint(assertBB);
    llvm::GlobalVariable *message =
        emitInternString("Unhandled exception in main!");
    llvm::Value *msgPtr = getPointerCast(message, getPointerType(getInt8Ty()));
    CRT->assertFail(Builder, msgPtr);
}

llvm::GlobalVariable *CodeGen::getEHInfo()
{
    static llvm::GlobalVariable *ehInfo = 0;

    if (ehInfo)
        return ehInfo;

    llvm::PointerType *bytePtr = getPointerType(getInt8Ty());

    std::vector<const llvm::Type*> members;
    members.push_back(getInt64Ty());
    members.push_back(bytePtr);
    llvm::StructType *theType = getStructTy(members);

    std::vector<llvm::Constant*> elts;
    elts.push_back(llvm::ConstantInt::get(getInt64Ty(), uint64_t(3)));
    elts.push_back(getPointerCast(emitInternString("Comma Exception"), bytePtr));
    llvm::Constant *theInfo = llvm::ConstantStruct::get(theType, elts);

    ehInfo = makeInternGlobal(theInfo, true);
    return ehInfo;
}

llvm::Function *CodeGen::getMemcpy64() const
{
    const llvm::Type *Tys[1] = { getInt64Ty() };
    return llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::memcpy, Tys, 1);
}

bool CodeGen::instancesPending()
{
    typedef WorkingSet::const_iterator iterator;
    iterator E = workList.end();
    for (iterator I = workList.begin(); I != E; ++I) {
        if (!I->second.isCompiled)
            return true;
    }
    return false;
}

void CodeGen::emitNextInstance()
{
    typedef WorkingSet::iterator iterator;
    iterator E = workList.end();
    for (iterator I = workList.begin(); I != E; ++I) {
        if (I->second.isCompiled)
            continue;
        CGCapsule = new CodeGenCapsule(*this, I->first);
        CGCapsule->emit();
        I->second.isCompiled = true;
        delete CGCapsule;
        return;
    }
}

bool CodeGen::extendWorklist(DomainInstanceDecl *instance)
{
    assert(!instance->isDependent() &&
           "Cannot extend the work list with dependent instances!");

    if (workList.count(instance))
        return false;

    // Do not add non-parameterized instances into the worklist.
    if (!instance->isParameterized())
        return false;

    WorkEntry &entry = workList[instance];

    entry.instance = instance;
    entry.isCompiled = false;

    // Iterate over the public subroutines provided by the given instance and
    // generate forward declarations for each of them.
    for (DeclRegion::DeclIter I = instance->beginDecls();
         I != instance->endDecls(); ++I) {
        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*I)) {
            std::string name = CGCapsule->getLinkName(srDecl);
            const llvm::FunctionType *fnTy =
                CGTypes->lowerSubroutine(srDecl);
            llvm::Function *fn = makeFunction(fnTy, name);
            insertGlobal(name, fn);
        }
    }
    return true;
}

bool CodeGen::insertGlobal(const std::string &linkName, llvm::GlobalValue *GV)
{
    assert(GV && "Cannot insert null values into the global table!");

    if (lookupGlobal(linkName))
        return false;

    globalTable[linkName] = GV;
    return true;
}

llvm::GlobalValue *CodeGen::lookupGlobal(const std::string &linkName) const
{
    StringGlobalMap::const_iterator iter = globalTable.find(linkName);

    if (iter != globalTable.end())
        return iter->second;
    return 0;
}

llvm::GlobalValue *CodeGen::lookupCapsuleInfo(Domoid *domoid) const
{
    std::string name = CGCapsule->getLinkName(domoid);
    StringGlobalMap::const_iterator iter = capsuleInfoTable.find(name);

    if (iter != capsuleInfoTable.end())
        return iter->second;
    else
        return 0;
}

llvm::GlobalVariable *CodeGen::emitInternString(const llvm::StringRef &elems,
                                                bool addNull,
                                                bool isConstant,
                                                const std::string &name)
{
    llvm::LLVMContext &ctx = getLLVMContext();
    llvm::Constant *string = llvm::ConstantArray::get(ctx, elems, addNull);
    return new llvm::GlobalVariable(*M, string->getType(), isConstant,
                                    llvm::GlobalValue::InternalLinkage,
                                    string, name);
}

llvm::GlobalVariable *CodeGen::emitInternArray(llvm::Constant *init,
                                               bool isConstant,
                                               const std::string &name)
{
    return new llvm::GlobalVariable(*M, init->getType(), isConstant,
                                    llvm::GlobalValue::InternalLinkage,
                                    init, name);
}

llvm::Constant *CodeGen::getNullPointer(const llvm::PointerType *Ty) const
{
    return llvm::ConstantPointerNull::get(Ty);
}

llvm::BasicBlock *CodeGen::makeBasicBlock(const std::string &name,
                                          llvm::Function *parent,
                                          llvm::BasicBlock *insertBefore) const
{
    llvm::LLVMContext &ctx = getLLVMContext();
    return llvm::BasicBlock::Create(ctx, name, parent, insertBefore);
}

llvm::PointerType *CodeGen::getPointerType(const llvm::Type *Ty) const
{
    return llvm::PointerType::getUnqual(Ty);
}

llvm::GlobalVariable *CodeGen::makeExternGlobal(llvm::Constant *init,
                                                bool isConstant,
                                                const std::string &name)

{
    return new llvm::GlobalVariable(*M, init->getType(), isConstant,
                                    llvm::GlobalValue::ExternalLinkage,
                                    init, name);
}

llvm::GlobalVariable *CodeGen::makeInternGlobal(llvm::Constant *init,
                                                bool isConstant,
                                                const std::string &name)

{
    return new llvm::GlobalVariable(*M, init->getType(), isConstant,
                                    llvm::GlobalValue::InternalLinkage,
                                    init, name);
}

llvm::Function *CodeGen::makeFunction(const llvm::FunctionType *Ty,
                                      const std::string &name)
{
    llvm::Function *fn =
        llvm::Function::Create(Ty, llvm::Function::ExternalLinkage, name, M);
    return fn;
}

llvm::Function *CodeGen::makeInternFunction(const llvm::FunctionType *Ty,
                                            const std::string &name)
{
    llvm::Function *fn =
        llvm::Function::Create(Ty, llvm::Function::InternalLinkage, name, M);
    return fn;
}

llvm::Constant *CodeGen::getPointerCast(llvm::Constant *constant,
                                        const llvm::PointerType *Ty) const
{
    return llvm::ConstantExpr::getPointerCast(constant, Ty);
}

llvm::Constant *
CodeGen::getConstantArray(const llvm::Type *elementType,
                          std::vector<llvm::Constant*> &elems) const
{
    llvm::ArrayType *arrayTy = llvm::ArrayType::get(elementType, elems.size());
    return llvm::ConstantArray::get(arrayTy, elems);
}

llvm::ConstantInt *CodeGen::getConstantInt(const llvm::IntegerType *type,
                                           uint64_t value)
{
    return llvm::ConstantInt::get(type, value);
}

llvm::ConstantInt *CodeGen::getConstantInt(const llvm::IntegerType *type,
                                           const llvm::APInt &value)
{
    llvm::Constant *res = 0;

    unsigned typeWidth = type->getBitWidth();
    unsigned valueWidth = value.getBitWidth();

    if (typeWidth == valueWidth)
        res = llvm::ConstantInt::get(type, value);
    else if (typeWidth > valueWidth) {
        llvm::APInt tmp(value);
        tmp.sext(typeWidth);
        res = llvm::ConstantInt::get(type, tmp);
    }
    else {
        llvm::APInt tmp(value);
        assert(tmp.getMinSignedBits() < typeWidth &&
               "Value to wide for supplied type!");

        tmp.trunc(typeWidth);
        res = llvm::ConstantInt::get(type, tmp);
    }

    return cast<llvm::ConstantInt>(res);
}
