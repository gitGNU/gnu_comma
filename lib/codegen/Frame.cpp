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

SRFrame::Subframe::Subframe(SRFrame *SRF, Subframe *parent)
    : SRF(SRF),
      parent(parent),
      restorePtr(0) { }

SRFrame::Subframe::~Subframe()
{
    emitStackrestore();
}

void SRFrame::Subframe::emitStacksave()
{
    if (restorePtr)
        return;

    llvm::Module *M;
    llvm::Function *save;

    M = SRF->getSRInfo()->getLLVMModule();
    save = llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::stacksave);
    restorePtr = SRF->getIRBuilder().CreateCall(save);
}

void SRFrame::Subframe::emitStackrestore()
{
    if (!restorePtr)
        return;

    if (SRF->getIRBuilder().GetInsertBlock()->getTerminator())
        return;

    llvm::Module *M;
    llvm::Function *restore;

    M = SRF->getSRInfo()->getLLVMModule();
    restore = llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::stackrestore);
    SRF->getIRBuilder().CreateCall(restore, restorePtr);
}

SRFrame::SRFrame(SRInfo *routineInfo, llvm::IRBuilder<> &Builder)
    : SRI(routineInfo),
      Builder(Builder),
      allocaBB(0),
      returnBB(0),
      currentSubframe(0),
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
    EntryMap::iterator I = entryTable.begin();
    EntryMap::iterator E = entryTable.end();
    for ( ; I != E; ++I)
        delete I->second;
}

void SRFrame::stacksave()
{
    // If there is no subframe then a stacksave is not needed since any
    // allocations are in a "straight line" code path.
    if (currentSubframe)
        currentSubframe->emitStacksave();
}

void SRFrame::pushFrame()
{
    currentSubframe = new Subframe(this, currentSubframe);
}

void SRFrame::popFrame()
{
    assert(currentSubframe && "Subframe imbalance!");
    Subframe *old = currentSubframe;
    currentSubframe = old->getParent();
    delete old;
}

void SRFrame::emitReturn()
{
    // Iterate over the set of subframes and emit a stackrestore for each
    // brefore branching to the return block.
    Subframe *cursor;
    for (cursor = currentSubframe; cursor; cursor = cursor->getParent())
        cursor->emitStackrestore();
    Builder.CreateBr(returnBB);
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

void SRFrame::associate(const ValueDecl *decl, activation::Tag tag,
                        llvm::Value *slot)
{
    EntryMap::value_type &pair = entryTable.FindAndConstruct(decl);
    ActivationEntry *&entry = pair.second;

    if (!entry)
        entry = new ActivationEntry();
    assert(!entry->find(tag) && "Decl aready associated with tag!");

    entry->add(new Property(tag, slot));
}

llvm::Value *SRFrame::lookup(const ValueDecl *decl, activation::Tag tag)
{
    EntryMap::iterator iter = entryTable.find(decl);

    if (iter != entryTable.end()) {
        ActivationEntry *entry = iter->second;
        if (Property *prop = entry->find(tag))
            return prop->getValue();
    }
    return 0;
}

void SRFrame::associate(const PrimaryType *type, activation::Tag tag,
                        llvm::Value *value)
{
    assert(tag != activation::Slot && "Cannot associate types with slots!");
    EntryMap::value_type &pair = entryTable.FindAndConstruct(type);
    ActivationEntry *&entry = pair.second;

    if (!entry)
        entry = new ActivationEntry();
    assert(!entry->find(tag) && "Type already associated with tag!");

    entry->add(new Property(tag, value));
}

llvm::Value *SRFrame::lookup(const PrimaryType *type, activation::Tag tag)
{
    EntryMap::iterator iter = entryTable.find(type);

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
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(savedBB);
}

void SRFrame::emitEpilogue()
{
    llvm::Function *Fn = SRI->getLLVMFunction();
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    assert(currentSubframe == 0 && "Subframe imbalance!");

    // Create the final return terminator.
    Builder.SetInsertPoint(returnBB);
    if (returnValue && !SRI->hasSRet()) {
        llvm::Value *V = Builder.CreateLoad(returnValue);
        Builder.CreateRet(V);
    }
    else
        Builder.CreateRetVoid();

    // Move the return block to the very end of the function.  Though by no
    // means necessary, this tweak does improve assembly readability a bit.
    llvm::BasicBlock *lastBB = &Fn->back();
    if (returnBB != lastBB)
        returnBB->moveAfter(lastBB);

    Builder.SetInsertPoint(savedBB);
}

llvm::BasicBlock *SRFrame::makeBasicBlock(const std::string &name,
                                          llvm::BasicBlock *insertBefore)
{
    llvm::Function *fn = getLLVMFunction();
    llvm::LLVMContext &ctx = fn->getContext();
    return llvm::BasicBlock::Create(ctx, name, fn, insertBefore);
}

