//===-- codegen/Frame.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Frame.h"

using namespace comma;
using namespace comma::activation;
using llvm::dyn_cast;

activation::Property *SRFrame::ActivationEntry::find(activation::Tag tag)
{
    typedef llvm::iplist<activation::Property>::iterator iterator;
    iterator I = plist.begin();
    iterator E = plist.end();
    for ( ; I != E; ++I) {
        if (I->getKind() == tag)
            return I;
    }
    return 0;
}

SRFrame::SRFrame(SRInfo *routineInfo, llvm::IRBuilder<> &Builder)
    : SRI(routineInfo),
      Builder(Builder),
      allocaBB(0),
      isSaved(false),
      restorePtr(0),
      returnBB(0),
      returnValue(0),
      implicitContext(0)
{
    llvm::Module *M = SRI->getLLVMModule();
    llvm::LLVMContext &Ctx = M->getContext();
    llvm::Function *Fn = SRI->getLLVMFunction();
    allocaBB = llvm::BasicBlock::Create(Ctx, "alloca", Fn);
    returnBB = llvm::BasicBlock::Create(Ctx, "return", Fn);

    Builder.SetInsertPoint(allocaBB);

    // If we are generating a function which is using the struct return calling
    // convention map the return value to the first parameter of this function.
    // If we are generating a vstack return we need not allocate a return value.
    // For simple calls, allocate a stack slot for the return value.
    if (SRI->isaFunction()) {
        if (SRI->hasSRet())
            returnValue = Fn->arg_begin();
        else if (!SRI->usesVRet())
            returnValue = createTemp(Fn->getReturnType());
    }

    // Populate the lookup tables with this functions arguments.
    injectSubroutineArgs();
}

SRFrame::~SRFrame()
{
    typedef EntryMap::iterator iterator;
    iterator I = entryTable.begin();
    iterator E = entryTable.end();
    for ( ; I != E; ++I)
        delete I->second;
}

void SRFrame::injectSubroutineArgs()
{
    SubroutineDecl *SRDecl = SRI->getDeclaration();
    llvm::Function *Fn = SRI->getLLVMFunction();
    llvm::Function::arg_iterator argI = Fn->arg_begin();

    // If this function uses the SRet convention, name the return argument.
    if (SRI->hasSRet()) {
        argI->setName("return.arg");
        ++argI;
    }

    // The next argument is the instance structure.  Name the arg "percent".
    argI->setName("percent");
    implicitContext = argI++;

    // For each formal argument, locate the corresponding llvm argument.  This
    // is mostly a one-to-one mapping except when unconstrained arrays are
    // present, in which case there are two arguments (one to the array and one
    // to the bounds).
    //
    // Set the name of each argument to match the corresponding formal.
    SubroutineDecl::const_param_iterator paramI = SRDecl->begin_params();
    SubroutineDecl::const_param_iterator paramE = SRDecl->end_params();
    for ( ; paramI != paramE; ++paramI, ++argI) {
        ParamValueDecl *param = *paramI;
        argI->setName(param->getString());
        associate(param, Slot, argI);

        if (ArrayType *arrTy = dyn_cast<ArrayType>(param->getType())) {
            if (!arrTy->isConstrained()) {
                ++argI;
                std::string boundName(param->getString());
                boundName += ".bounds";
                argI->setName(boundName);
                associate(param, Bounds, argI);
            }
        }
    }
}

llvm::Value *SRFrame::createTemp(const llvm::Type *type)
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(allocaBB);
    llvm::Value *slot = Builder.CreateAlloca(type);
    Builder.SetInsertPoint(savedBB);
    return slot;
}

void SRFrame::associate(ValueDecl *decl, activation::Tag tag, llvm::Value *slot)
{
    EntryMap::value_type &pair = entryTable.FindAndConstruct(decl);
    ActivationEntry *&entry = pair.second;

    if (!entry)
        entry = new ActivationEntry();
    assert(!entry->find(tag) && "Decl aready associated with tag!");

    entry->add(new Property(tag, slot));
}

llvm::Value *SRFrame::lookup(ValueDecl *decl, activation::Tag tag)
{
    EntryMap::iterator iter = entryTable.find(decl);

    if (iter != entryTable.end()) {
        ActivationEntry *entry = iter->second;
        if (Property *prop = entry->find(tag))
            return prop->getValue();
    }
    return 0;
}

void SRFrame::emitPrologue(llvm::BasicBlock *bodyBB)
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(allocaBB);

    // If this frame was marked as a stacksaved region, insert a stacksave
    // instruction at the end of the alloca block, then branch to the body.
    if (isSaved) {
        llvm::Module *M = SRI->getLLVMModule();
        llvm::Function *ssave =
            llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::stacksave);
        restorePtr = Builder.CreateCall(ssave);
    }

    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(savedBB);
}

void SRFrame::emitEpilogue()
{
    llvm::Function *Fn = SRI->getLLVMFunction();
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(returnBB);

    // If this frame was marked as a stacksaved region, insert a stackrestore
    // instruction into returnBB.
    if (isSaved) {
        llvm::Module *M = SRI->getLLVMModule();
        llvm::Function *srestore =
            llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::stackrestore);
        Builder.CreateCall(srestore, restorePtr);
    }

    // Create the final return terminator.
    if (returnValue && !SRI->hasSRet()) {
        llvm::Value *V = Builder.CreateLoad(returnValue);
        Builder.CreateRet(V);
    }
    else
        Builder.CreateRetVoid();

    Builder.SetInsertPoint(savedBB);

    // Move the return block to the very end of the function.  Though by no
    // means necessary, this tweak does improve assembly readability a bit.
    llvm::BasicBlock *lastBB = &Fn->back();
    if (returnBB != lastBB)
        returnBB->moveAfter(lastBB);
}
