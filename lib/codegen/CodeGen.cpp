//===-- codegen/CodeGen.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGen.h"
#include "SRInfo.h"
#include "CGContext.h"
#include "CodeGenRoutine.h"
#include "CommaRT.h"
#include "DependencySet.h"
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
      moduleName(0) { }

CodeGen::~CodeGen()
{
    delete CRT;
}

void CodeGen::emitCompilationUnit(CompilationUnit *cunit)
{
    // Generate capsule info objects for each dependency.
    typedef CompilationUnit::dep_iterator dep_iterator;
    for (dep_iterator I = cunit->begin_dependencies(),
             E = cunit->end_dependencies(); I != E; ++I) {
        Decl *decl = *I;
        if (Domoid *domoid = dyn_cast<Domoid>(decl)) {
            std::string linkName = mangle::getLinkName(domoid);
            capsuleInfoTable[linkName] = CRT->declareCapsule(domoid);
        }

        if (DomainDecl *domain = dyn_cast<DomainDecl>(decl)) {
            InstanceInfo *info = createInstanceInfo(domain->getInstance());
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
    if (DomainDecl *domain = dyn_cast<DomainDecl>(decl)) {
        DependencySet *DS = new DependencySet(domain);
        dependenceTable[domain] = DS;

        // Generate an InstanceInfo object for this domain.
        InstanceInfo *info = createInstanceInfo(domain->getInstance());
        emitCapsule(info);
        capsuleInfoTable[info->getLinkName()] = CRT->defineCapsule(domain);
    }
    else if (FunctorDecl *functor = dyn_cast<FunctorDecl>(decl)) {
        DependencySet *DS = new DependencySet(functor);
        dependenceTable[functor] = DS;

        std::string name = mangle::getLinkName(functor);
        capsuleInfoTable[name] = CRT->defineCapsule(functor);
    }
    else
        return;

    // Continuously compile from the set of required instances so long as there
    // exist entries which need to be codegened.
    while (instancesPending())
        emitNextInstance();
}

void CodeGen::emitCapsule(InstanceInfo *info)
{
    CGContext CGC(*this, info);

    // Codegen each subroutine.
    const AddDecl *add = info->getDefinition()->getImplementation();
    for (DeclRegion::ConstDeclIter I = add->beginDecls(), E = add->endDecls();
         I != E; ++I) {
        if (SubroutineDecl *SRD = dyn_cast<SubroutineDecl>(*I)) {
            SRInfo *SRI = getSRInfo(info->getInstanceDecl(), SRD);
            CodeGenRoutine CGR(CGC, SRI);
            CGR.emit();
        }
    }
    info->markAsCompiled();
}

void CodeGen::emitEntry(ProcedureDecl *pdecl)
{
    // Basic sanity checks on the declaration.
    assert(pdecl->getArity() == 0 && "Entry procedures must be nullary!");

    // Get the procedures declarative region. This must be an instance of a
    // non-parameterized domain.
    DeclRegion *region = pdecl->getDeclRegion();
    DomainInstanceDecl *context = cast<DomainInstanceDecl>(region);
    assert(!context->isParameterized() &&
           "Cannot call entry procedures in a parameterized context!");

    // Lookup the previously codegened function for this decl.
    SRInfo *info = getSRInfo(context, pdecl);
    llvm::Value *func = info->getLLVMFunction();

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
    llvm::Value *eh_person = CRT->getEHPersonality();
    llvm::Function *eh_except = getEHExceptionIntrinsic();
    llvm::Function *eh_select = getEHSelectorIntrinsic();

    // The exception object produced by a call to _comma_raise.
    llvm::Value *except = Builder.CreateCall(eh_except);

    // Build an exception selector with a null exception info entry.  This is a
    // catch-all selector.
    args.clear();
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

InstanceInfo *CodeGen::createInstanceInfo(DomainInstanceDecl *instance)
{
    assert(!lookupInstanceInfo(instance) &&
           "Instance already has associated info!");

    InstanceInfo *info = new InstanceInfo(*this, instance);
    instanceTable[instance] = info;
    return info;
}

bool CodeGen::instancesPending() const
{
    typedef InstanceMap::const_iterator iterator;
    iterator E = instanceTable.end();
    for (iterator I = instanceTable.begin(); I != E; ++I) {
        if (!I->second->isCompiled())
            return true;
    }
    return false;
}

void CodeGen::emitNextInstance()
{
    typedef InstanceMap::iterator iterator;
    iterator E = instanceTable.end();
    for (iterator I = instanceTable.begin(); I != E; ++I) {
        if (I->second->isCompiled())
            continue;
        emitCapsule(I->second);
        return;
    }
}

bool CodeGen::extendWorklist(DomainInstanceDecl *instance)
{
    assert(!instance->isDependent() && "Cannot codegen dependent instances!");

    // If there is already and entry for this instance we will codegen it
    // eventually (if we have not already).
    if (lookupInstanceInfo(instance))
        return false;

    createInstanceInfo(instance);
    return true;
}

SRInfo *CodeGen::getSRInfo(DomainInstanceDecl *instance,
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

llvm::GlobalValue *CodeGen::lookupCapsuleInfo(const Domoid *domoid) const
{
    std::string name = mangle::getLinkName(domoid);
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

llvm::Function *CodeGen::makeFunction(const DomainInstanceDecl *instance,
                                      const SubroutineDecl *srDecl,
                                      CodeGenTypes &CGT)
{
    const llvm::FunctionType *fnTy = CGT.lowerSubroutine(srDecl);
    std::string fnName = mangle::getLinkName(instance, srDecl);
    llvm::GlobalValue::LinkageTypes linkTy;
    llvm::Function *fn;

    // All instances should be fully resolved.
    assert(!instance->isDependent() && "Unexpected dependent instance!");

    // If this is a functor instance all functions must be defined with
    // WeakODRLinkage (One Definition Rule, as in C++).  Otherwise use external
    // linkage.
    if (instance->isParameterized())
        linkTy = llvm::GlobalValue::WeakODRLinkage;
    else
        linkTy = llvm::GlobalValue::ExternalLinkage;
    fn = makeFunction(fnTy, fnName, linkTy);

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

const DependencySet &CodeGen::getDependencySet(const Domoid *domoid)
{
    DependenceMap::value_type &entry = dependenceTable.FindAndConstruct(domoid);

    if (entry.second)
        return *entry.second;

    DependencySet *DS = new DependencySet(domoid);
    entry.second = DS;
    return *DS;
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
