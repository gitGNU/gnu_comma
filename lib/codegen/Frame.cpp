//===-- codegen/Frame.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenRoutine.h"
#include "Frame.h"

using namespace comma;
using namespace comma::activation;
using llvm::dyn_cast;

activation::Property *Frame::ActivationEntry::find(activation::Tag tag)
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

Subframe::Subframe(Kind kind, Frame *SRF, Subframe *parent,
                   const llvm::Twine &name)
    : kind(kind),
      SRF(SRF),
      parent(parent),
      restorePtr(0),
      landingPad(0),
      entryBB(SRF->makeBasicBlock(name)),
      mergeBB(SRF->makeBasicBlock("merge")) { }

Subframe::Subframe(Kind kind, Frame *SRF, Subframe *parent,
                   llvm::BasicBlock *entry, llvm::BasicBlock *merge)
    : kind(kind),
      SRF(SRF),
      parent(parent),
      restorePtr(0),
      landingPad(0),
      entryBB(entry),
      mergeBB(merge) { }

Subframe::~Subframe()
{
    emitStackrestore();
}

void Subframe::emitStacksave()
{
    if (restorePtr)
        return;

    llvm::Module *M;
    llvm::Function *stacksave;
    llvm::BasicBlock *savedBB;
    llvm::IRBuilder<> &Builder = SRF->getIRBuilder();

    M = SRF->getSRInfo()->getLLVMModule();
    stacksave = llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::stacksave);

    savedBB = Builder.GetInsertBlock();
    Builder.SetInsertPoint(entryBB, entryBB->begin());
    restorePtr = SRF->getIRBuilder().CreateCall(stacksave);
    Builder.SetInsertPoint(savedBB);
}

void Subframe::emitStackrestore()
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

void Subframe::addLandingPad()
{
    if (landingPad)
        return;
    landingPad = SRF->makeBasicBlock("landingpad");
}

Frame::Frame(SRInfo *routineInfo,
             CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder)
    : SRI(routineInfo),
      Builder(Builder),
      allocaBB(0),
      returnBB(0),
      currentSubframe(0),
      returnValue(0)
{
    llvm::Module *M = SRI->getLLVMModule();
    llvm::LLVMContext &Ctx = M->getContext();
    llvm::Function *Fn = SRI->getLLVMFunction();
    allocaBB = llvm::BasicBlock::Create(Ctx, "alloa", Fn);
    returnBB = llvm::BasicBlock::Create(Ctx, "return", Fn);

    Builder.SetInsertPoint(allocaBB);

    // Populate the lookup tables with this functions arguments.
    injectSubroutineArgs(CGR);

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

    // Push the inital subframe.
    pushFrame(Subframe::Entry, allocaBB, returnBB);
}

Frame::~Frame()
{
    popFrame();

    EntryMap::iterator I = entryTable.begin();
    EntryMap::iterator E = entryTable.end();
    for ( ; I != E; ++I)
        delete I->second;
}

void Frame::stacksave()
{
    currentSubframe->emitStacksave();
}

void Frame::addLandingPad()
{
    currentSubframe->addLandingPad();
}

bool Frame::hasLandingPad()
{
    return getLandingPad() != 0;
}

llvm::BasicBlock *Frame::getLandingPad()
{
    llvm::BasicBlock *lpad = 0;
    Subframe *cursor = currentSubframe;
    while (cursor) {
        if ((lpad = cursor->getLandingPad()))
            break;
        cursor = cursor->getParent();
    }
    return lpad;
}

void Frame::removeLandingPad()
{
    Subframe *cursor = currentSubframe;
    while (cursor) {
        if (cursor->getLandingPad()) {
            cursor->removeLandingPad();
            break;
        }
        cursor = cursor->getParent();
    }
}

Subframe *Frame::findFirstSubframe(Subframe::Kind kind)
{
    Subframe *cursor = currentSubframe;
    while (cursor) {
        if (cursor->getKind() == kind)
            return cursor;
        cursor = cursor->getParent();
    }
    return 0;
}

Subframe *Frame::findFirstNamedSubframe(const llvm::Twine &name)
{
    Subframe *cursor = currentSubframe;
    std::string target = name.str();
    while (cursor) {
        llvm::StringRef ref = cursor->getName();
        if (ref.equals(target))
            return cursor;
        cursor = cursor->getParent();
    }
    return 0;
}

llvm::BasicBlock *Frame::pushFrame(Subframe::Kind kind, const llvm::Twine &name)
{
    currentSubframe = new Subframe(kind, this, currentSubframe, name);
    return currentSubframe->getEntryBB();
}

void Frame::pushFrame(Subframe::Kind kind,
                      llvm::BasicBlock *entryBB, llvm::BasicBlock *mergeBB)
{
    if (!mergeBB)
        mergeBB = makeBasicBlock("merge");
    currentSubframe =
        new Subframe(kind, this, currentSubframe, entryBB, mergeBB);
}

void Frame::popFrame()
{
    assert(currentSubframe && "Subframe imbalance!");
    Subframe *old = currentSubframe;
    currentSubframe = old->getParent();
    delete old;
}

void Frame::emitReturn()
{
    // Iterate over the set of subframes and emit a stackrestore for each before
    // branching to the return block.  However, ignore the first implicit
    // subframe since any stacksaves are redundant.
    Subframe *cursor;
    for (cursor = currentSubframe; cursor; cursor = cursor->getParent())
        if (cursor->getParent())
            cursor->emitStackrestore();
    Builder.CreateBr(returnBB);
}

void Frame::injectSubroutineArgs(CodeGenRoutine &CGR)
{
    SubroutineDecl *SRDecl = SRI->getDeclaration();
    llvm::Function *Fn = SRI->getLLVMFunction();
    llvm::Function::arg_iterator argI = Fn->arg_begin();

    // If this function uses the SRet convention, name the return argument.
    if (SRI->hasSRet()) {
        argI->setName("return.arg");
        ++argI;
    }

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

        Type *paramTy = CGR.resolveType(param->getType());
        if (ArrayType *arrTy = dyn_cast<ArrayType>(paramTy)) {
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

llvm::Value *Frame::createTemp(const llvm::Type *type)
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(allocaBB);
    llvm::Value *slot = Builder.CreateAlloca(type);
    Builder.SetInsertPoint(savedBB);
    return slot;
}

void Frame::associate(const ValueDecl *decl, activation::Tag tag,
                      llvm::Value *slot)
{
    EntryMap::value_type &pair = entryTable.FindAndConstruct(decl);
    ActivationEntry *&entry = pair.second;

    if (!entry)
        entry = new ActivationEntry();
    assert(!entry->find(tag) && "Decl aready associated with tag!");

    entry->add(new Property(tag, slot));
}

llvm::Value *Frame::lookup(const ValueDecl *decl, activation::Tag tag)
{
    EntryMap::iterator iter = entryTable.find(decl);

    if (iter != entryTable.end()) {
        ActivationEntry *entry = iter->second;
        if (Property *prop = entry->find(tag))
            return prop->getValue();
    }
    return 0;
}

void Frame::associate(const PrimaryType *type, activation::Tag tag,
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

llvm::Value *Frame::lookup(const PrimaryType *type, activation::Tag tag)
{
    EntryMap::iterator iter = entryTable.find(type);

    if (iter != entryTable.end()) {
        ActivationEntry *entry = iter->second;
        if (Property *prop = entry->find(tag))
            return prop->getValue();
    }
    return 0;
}

void Frame::emitPrologue(llvm::BasicBlock *bodyBB)
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();
    Builder.SetInsertPoint(allocaBB);
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(savedBB);
}

void Frame::emitEpilogue()
{
    llvm::Function *Fn = SRI->getLLVMFunction();
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    assert(currentSubframe->getParent() == 0 && "Subframe imbalance!");

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

llvm::BasicBlock *Frame::makeBasicBlock(const llvm::Twine &name,
                                        llvm::BasicBlock *insertBefore)
{
    llvm::Function *fn = getLLVMFunction();
    llvm::LLVMContext &ctx = fn->getContext();
    return llvm::BasicBlock::Create(ctx, name, fn, insertBefore);
}

