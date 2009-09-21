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
    std::vector<const llvm::Type*> args;
    args.push_back(getInt32Ty());
    args.push_back(getPointerType(getPointerType(getInt8Ty())));
    llvm::FunctionType *entryTy =
        llvm::FunctionType::get(getInt32Ty(), args, false);

    // Get an external function to populate with the entry code.
    //
    // FIXME: The name of main function is a target dependent operation.
    llvm::Function *entry = makeFunction(entryTy, "main");

    llvm::IRBuilder<> Builder(getLLVMContext());
    llvm::BasicBlock *entryBB = makeBasicBlock("entry", entry);
    Builder.SetInsertPoint(entryBB);

    // Lookup the domain info object for the needed domain.
    llvm::GlobalValue *domainInfo = lookupCapsuleInfo(context->getDefinition());
    assert(domainInfo && "Could not find domain_info for entry function!");

    // Build the get_domain call.  Since this domain is not parameterized, only
    // the domain_info object is required as an argument.
    llvm::Value *domainInstance = CRT->getDomain(Builder, domainInfo);

    // Call our entry function with the generated instance as its only argument.
    Builder.CreateCall(func, domainInstance);

    // Return a zero exit status.
    Builder.CreateRet(llvm::ConstantInt::get(getInt32Ty(), uint64_t(0)));
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

llvm::Constant *CodeGen::emitStringLiteral(const std::string &str,
                                           bool isConstant,
                                           const std::string &name)
{
    llvm::LLVMContext &ctx = getLLVMContext();
    llvm::Constant *stringConstant = llvm::ConstantArray::get(ctx, str, true);
    return new llvm::GlobalVariable(*M, stringConstant->getType(), isConstant,
                                    llvm::GlobalValue::InternalLinkage,
                                    stringConstant, name);
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

    // FIXME:  For now, Comma functions never thow.  When they do, this method
    // can be extended with an additional boolean flag indicating if the
    // function throws.
    fn->setDoesNotThrow();
    return fn;
}

llvm::Function *CodeGen::makeInternFunction(const llvm::FunctionType *Ty,
                                            const std::string &name)
{
    llvm::Function *fn =
        llvm::Function::Create(Ty, llvm::Function::InternalLinkage, name, M);

    // FIXME:  For now, Comma functions never thow.  When they do, this method
    // can be extended with an additional boolean flag indicating if the
    // function throws.
    fn->setDoesNotThrow();
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
