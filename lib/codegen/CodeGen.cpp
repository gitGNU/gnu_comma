//===-- codegen/CodeGen.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGen.h"
#include "SRInfo.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "InstanceInfo.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Cunit.h"
#include "comma/ast/Decl.h"
#include "comma/basic/TextManager.h"
#include "comma/codegen/Mangle.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CodeGen::CodeGen(llvm::Module *M, const llvm::TargetData &data,
                 TextManager &manager, AstResource &resource)
    : M(M),
      TD(data),
      Manager(manager),
      Resource(resource),
      CRT(new CommaRT(*this)),
      CGT(new CodeGenTypes(*this)),
      moduleName(0) { }

CodeGen::~CodeGen()
{
    delete CRT;
}

void CodeGen::emitCompilationUnit(CompilationUnit *cunit)
{
    // Generate instance info objects for each non-parameterized dependency.
    typedef CompilationUnit::dep_iterator dep_iterator;
    for (dep_iterator I = cunit->begin_dependencies(),
             E = cunit->end_dependencies(); I != E; ++I) {
        if (PackageDecl *package = dyn_cast<PackageDecl>(*I)) {
            InstanceInfo *info = createInstanceInfo(package->getInstance());
            info->markAsCompiled();
        }
    }

    // Codegen each declaration.
    typedef CompilationUnit::decl_iterator decl_iterator;
    for (decl_iterator I = cunit->begin_declarations(),
             E = cunit->end_declarations(); I != E; ++I) {
        emitToplevelDecl(*I);
    }
}

void CodeGen::emitToplevelDecl(Decl *decl)
{
    InstanceInfo *info = 0;

    if (PackageDecl *package = dyn_cast<PackageDecl>(decl))
        info = createInstanceInfo(package->getInstance());

    if (info) {
        emitPackage(info);

        // Continuously compile from the set of required instances so long as
        // there exist entries which need to be codegened.
        while (instancesPending())
            emitNextInstance();
    }
}

void CodeGen::emitPackage(InstanceInfo *info)
{
    // Codegen each subroutine.
    IInfo = info;
    PkgInstanceDecl *instance = IInfo->getInstance();
    const BodyDecl *body = instance->getDefinition()->getImplementation();

    if (body) {
        typedef DeclRegion::ConstDeclIter iterator;
        for (iterator I = body->beginDecls(), E = body->endDecls();
             I != E; ++I) {
            if (SubroutineDecl *SRD = dyn_cast<SubroutineDecl>(*I)) {
                SRInfo *SRI = getSRInfo(instance, SRD);
                CodeGenRoutine CGR(*this, SRI);
                CGR.emit();
            }
        }
    }
    IInfo->markAsCompiled();
    IInfo = 0;
}

void CodeGen::emitEntry(ProcedureDecl *pdecl)
{
    // Basic sanity checks on the declaration.
    assert(pdecl->getArity() == 0 && "Entry procedures must be nullary!");

    // Lookup the generated function.
    DeclRegion *region = pdecl->getDeclRegion();
    PkgInstanceDecl *package = cast<PkgInstanceDecl>(region);
    llvm::Value *func = getSRInfo(package, pdecl)->getLLVMFunction();

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

    // Invoke our entry function.
    Builder.CreateInvoke(func, returnBB, landingBB);

    // Switch to normal return.
    Builder.SetInsertPoint(returnBB);
    Builder.CreateRet(llvm::ConstantInt::get(getInt32Ty(), uint64_t(0)));

    // Switch to landing pad.
    Builder.SetInsertPoint(landingBB);
    llvm::Value *eh_person = CRT->getEHPersonality();
    llvm::Function *eh_except = getEHExceptionIntrinsic();
    llvm::Function *eh_select = getEHSelectorIntrinsic();

    // The exception object produced by a call to _comma_raise.
    llvm::Value *except = Builder.CreateCall(eh_except);

    // Build an exception selector with a null exception info entry.  This is a
    // catch-all selector.
    llvm::SmallVector<llvm::Value*, 3> args;
    args.push_back(except);
    args.push_back(eh_person);
    args.push_back(getNullPointer(getInt8PtrTy()));
    Builder.CreateCall(eh_select, args.begin(), args.end());

    // Call into _comma_unhandled_exception and pass it the exception object.
    // This function never returns.
    CRT->unhandledException(Builder, except);
}

llvm::Function *CodeGen::getMemcpy64() const
{
    const llvm::Type *Tys[1] = { getInt64Ty() };
    return llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::memcpy, Tys, 1);
}

llvm::Function *CodeGen::getMemcpy32() const
{
    const llvm::Type *Tys[1] = { getInt32Ty() };
    return llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::memcpy, Tys, 1);
}

llvm::Function *CodeGen::getMemset32() const
{
    const llvm::Type *Tys[1] = { getInt32Ty() };
    return llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::memset, Tys, 1);
}

llvm::Function *CodeGen::getEHExceptionIntrinsic() const
{
    return getLLVMIntrinsic(llvm::Intrinsic::eh_exception);
}

llvm::Function *CodeGen::getEHSelectorIntrinsic() const
{
    return getLLVMIntrinsic(llvm::Intrinsic::eh_selector);
}

llvm::Function *CodeGen::getEHTypeidIntrinsic() const
{
    return getLLVMIntrinsic(llvm::Intrinsic::eh_typeid_for);
}

InstanceInfo *CodeGen::createInstanceInfo(PkgInstanceDecl *instance)
{
    assert(!lookupInstanceInfo(instance) &&
           "Instance already has associated info!");

    InstanceInfo *info = new InstanceInfo(*this, instance);
    InstanceTable[instance] = info;
    return info;
}

bool CodeGen::instancesPending() const
{
    typedef InstanceMap::const_iterator iterator;
    iterator E = InstanceTable.end();
    for (iterator I = InstanceTable.begin(); I != E; ++I) {
        if (!I->second->isCompiled())
            return true;
    }
    return false;
}

void CodeGen::emitNextInstance()
{
    typedef InstanceMap::iterator iterator;
    iterator E = InstanceTable.end();
    for (iterator I = InstanceTable.begin(); I != E; ++I) {
        if (I->second->isCompiled())
            continue;
        emitPackage(I->second);
        return;
    }
}

bool CodeGen::extendWorklist(PkgInstanceDecl *instance)
{
    // If there is already and entry for this instance we will codegen it
    // eventually (if we have not already).
    if (lookupInstanceInfo(instance))
        return false;

    createInstanceInfo(instance);
    return true;
}

SRInfo *CodeGen::getSRInfo(PkgInstanceDecl *instance,
                           SubroutineDecl *srDecl)
{
    InstanceInfo *IInfo = getInstanceInfo(instance);
    return IInfo->getSRInfo(srDecl);
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
    return M->getGlobalVariable(linkName);
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

llvm::BasicBlock *CodeGen::makeBasicBlock(const std::string &name,
                                          llvm::Function *parent,
                                          llvm::BasicBlock *insertBefore) const
{
    llvm::LLVMContext &ctx = getLLVMContext();
    return llvm::BasicBlock::Create(ctx, name, parent, insertBefore);
}

llvm::GlobalVariable *CodeGen::makeExternGlobal(llvm::Constant *init,
                                                bool isConstant,
                                                const std::string &name)

{
    return new llvm::GlobalVariable(*M, init->getType(), isConstant,
                                    llvm::GlobalValue::ExternalLinkage,
                                    init, name);
}

llvm::GlobalVariable *CodeGen::makeExternGlobal(const llvm::Type *type,
                                                bool isConstant,
                                                const std::string &name)
{
    return new llvm::GlobalVariable(*M, type, isConstant,
                                    llvm::GlobalValue::ExternalLinkage,
                                    0, name);
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
                                      const std::string &name,
                                      llvm::GlobalValue::LinkageTypes linkTy)
{
    llvm::Function *fn =
        llvm::Function::Create(Ty, linkTy, name, M);
    return fn;
}

llvm::Function *CodeGen::makeFunction(const PkgInstanceDecl *instance,
                                      const SubroutineDecl *srDecl,
                                      CodeGenTypes &CGT)
{
    const llvm::FunctionType *fnTy = CGT.lowerSubroutine(srDecl);
    std::string fnName = mangle::getLinkName(instance, srDecl);
    llvm::Function *fn;

    // FIXME: When we support generic packages the linkage type must be
    // WeakODRLinkage.
    fn = makeFunction(fnTy, fnName, llvm::GlobalValue::ExternalLinkage);

    // Mark the function as sret if needed.
    if (CGT.getConvention(srDecl) == CodeGenTypes::CC_Sret)
        fn->addAttribute(1, llvm::Attribute::StructRet);
    return fn;
}

llvm::Function *CodeGen::makeInternFunction(const llvm::FunctionType *Ty,
                                            const std::string &name)
{
    llvm::Function *fn =
        llvm::Function::Create(Ty, llvm::Function::InternalLinkage, name, M);
    return fn;
}

llvm::ConstantInt *CodeGen::getConstantInt(const llvm::IntegerType *type,
                                           const llvm::APInt &value) const
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

llvm::Constant *CodeGen::getModuleName()
{
    // Lazily construct the global on first call to this method.
    if (moduleName)
        return moduleName;

    moduleName = emitInternString(M->getModuleIdentifier());
    moduleName = getPointerCast(moduleName, getInt8PtrTy());
    return moduleName;
}

SourceLocation CodeGen::getSourceLocation(Location loc)
{
    return getTextManager().getSourceLocation(loc);
}

//===----------------------------------------------------------------------===//
// Generator

Generator *Generator::create(llvm::Module *M, const llvm::TargetData &data,
                             TextManager &manager, AstResource &resource)
{
    return new CodeGen(M, data, manager, resource);
}
