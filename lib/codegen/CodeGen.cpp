//===-- codegen/CodeGen.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

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
