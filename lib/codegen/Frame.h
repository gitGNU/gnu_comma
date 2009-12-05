//===-- codegen/Frame.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_FRAME_HDR_GUARD
#define COMMA_CODEGEN_FRAME_HDR_GUARD

#include "SRInfo.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/ilist_node.h"
#include "llvm/Support/IRBuilder.h"

namespace comma {

namespace activation {

enum Tag {
    Slot,
    Bounds,
    Length,
};

class Property : public llvm::ilist_node<Property> {
public:
    Property() : kind(Slot), value(0) { }

    Property(Tag kind, llvm::Value *value)
        : kind(kind), value(value) { }

    Tag getKind() const { return kind; }

    llvm::Value *getValue() { return value; }

    static bool classof(const Property *prop) { return true; }

private:
    Tag kind;
    llvm::Value *value;
};

}; // end activation namespace.

class SRFrame {

public:
    SRFrame(SRInfo *routineInfo, llvm::IRBuilder<> &Builder);

    ~SRFrame();

    /// Returns the SRInfo object this SRFrame represents.
    SRInfo *getSRInfo() { return SRI; }

    /// Returns the subroutine associated with this frame.
    SubroutineDecl *getDeclaration() { return SRI->getDeclaration(); }

    /// Returns the LLVM function managed by this frame.
    llvm::Function *getLLVMFunction() { return SRI->getLLVMFunction(); }

    /// Returns the IRBuilder which should be used to emit code for this
    /// subroutine.
    llvm::IRBuilder<> &getIRBuilder() { return Builder; }

    /// \name Allocation methods.
    //@{
    llvm::Value *createTemp(const llvm::Type *type);

    void associate(const ValueDecl *decl, activation::Tag tag,
                   llvm::Value *slot);

    llvm::Value *lookup(const ValueDecl *decl, activation::Tag tag);

    llvm::Value *createEntry(const ValueDecl *decl, activation::Tag tag,
                             const llvm::Type *type) {
        llvm::Value *slot = createTemp(type);
        associate(decl, tag, slot);
        return slot;
    }
    //@}

    /// \name Type insert/lookup.
    //@{
    void associate(const PrimaryType *type, activation::Tag tag,
                   llvm::Value *value);

    llvm::Value *lookup(const PrimaryType *type, activation::Tag tag);
    //@}

    /// Marks this frame as a stacksave frame.
    void stacksave() { isSaved = true; }

    void emitReturn() { Builder.CreateBr(returnBB); }

    llvm::Value *getReturnValue() { return returnValue; }

    llvm::Value *getImplicitContext() { return implicitContext; }

    void emitPrologue(llvm::BasicBlock *bodyBB);
    void emitEpilogue();

private:
    /// The SRInfo object associated with the subroutine this frame is
    /// representing.
    SRInfo *SRI;

    /// IRBuilder used to generate the code for this subroutine.
    llvm::IRBuilder<> &Builder;

    /// A basic block holding all alloca'd data in this subroutine.
    llvm::BasicBlock *allocaBB;

    /// True when this frame has been marked as a stacksave frame.
    bool isSaved;

    /// When isSaved is true, this member holds onto the stackrestore pointer.
    llvm::Value *restorePtr;

    /// The final return basic block;
    llvm::BasicBlock *returnBB;

    /// The return value for this function.  This member is null when we are
    /// generating an procedure or function using the sret calling convention.
    llvm::Value *returnValue;

    /// The implicit instance parameter for this function.
    llvm::Value *implicitContext;

    /// The following structure is used to hold information about alloca'd
    /// objects within an SRFrame.
    class ActivationEntry {

    public:
        ActivationEntry() : plist() { }

        activation::Property *find(activation::Tag tag);

        void add(activation::Property *prop) { plist.push_front(prop); }

    private:
        ActivationEntry(const ActivationEntry &); // Do not implement.
        void operator=(const ActivationEntry &);  // Likewise.

        llvm::iplist<activation::Property> plist;
    };

    // Map from Comma Decl's and Type's to AllocaEntry's.
    typedef llvm::DenseMap<const Ast*, ActivationEntry*> EntryMap;
    EntryMap entryTable;

    /// Initialize the implicit first parameter.  Also, name the llvm arguments
    /// after the source formals, and populate the lookup tables such that a
    /// search for a parameter decl yields the corresponding llvm value.
    void injectSubroutineArgs();
};

} // end comma namespace.

#endif
