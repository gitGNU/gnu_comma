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

class CodeGenRoutine;

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
    SRFrame(SRInfo *routineInfo,
            CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder);

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

    /// Creates a basic block for this frame.
    llvm::BasicBlock *makeBasicBlock(const std::string &name = "",
                                     llvm::BasicBlock *insertBefore = 0);

    /// \name Subframe Methods.
    //@{
    /// Pushes a new subframe and makes it current.
    void pushFrame();

    /// Pop's the current subframe.
    void popFrame();

    /// Marks the current subframe as a stacksave frame.
    void stacksave();

    /// Adds a landing pad to the current subframe.
    void addLandingPad();
    //@}

    /// Returns true if there exists a subframe with an active landing pad.
    bool hasLandingPad();

    /// Returns the basic block for use as a landing pad, or null if
    /// hasLandingPad returns false.
    llvm::BasicBlock *getLandingPad();

    /// Removes the innermost landing pad from the subframe stack, if any.
    void removeLandingPad();

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

    template <class Iter>
    llvm::Value *emitCall(llvm::Value *fn, Iter begin, Iter end) {
        llvm::Value *result = 0;
        if (llvm::BasicBlock *lpad = getLandingPad()) {
            llvm::BasicBlock *norm = makeBasicBlock("invoke.normal");
            result = Builder.CreateInvoke(fn, norm, lpad, begin, end);
            Builder.SetInsertPoint(norm);
        }
        else
            result = Builder.CreateCall(fn, begin, end);
        return result;
    }

    void emitReturn();

    llvm::Value *getReturnValue() { return returnValue; }

    llvm::Value *getImplicitContext() { return implicitContext; }

    void emitPrologue(llvm::BasicBlock *bodyBB);
    void emitEpilogue();

private:
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

    class Subframe {

    public:
        Subframe(SRFrame *context, Subframe *parent);
        ~Subframe();

        /// Returns true if this is a nested subframe.
        bool hasParent() const { return parent != 0; }

        /// Returns the parent of this frame, or null if hasParent() returns
        /// false.
        Subframe *getParent() { return parent; }

        /// On the first call emits a stacksave instruction.  Subsequent calls
        /// have no effect.
        void emitStacksave();

        /// Emits a stackrestore iff there was a previous call to
        /// emitStacksave().
        void emitStackrestore();

        /// Associates a landing pad with this subframe.
        void addLandingPad();

        /// Removes the associated landing pad from this subframe.
        void removeLandingPad() { landingPad = 0; }

        /// Returns the landing pad basic block associated with this subframe as
        /// a result of a call to requireLandingPad, or null if
        /// requireLandingPad has yet to be called on this subframe.
        llvm::BasicBlock *getLandingPad() { return landingPad; }

    private:
        /// Back-link to the managing SRFrame.
        SRFrame *SRF;

        /// The Subframe which contains this one, or null if this is a top-level
        /// frame.
        Subframe *parent;

        /// Non-null when emitStacksave is called on this Subframe.  Holds the
        /// restoration pointer for use in a stackrestore call.
        llvm::Value *restorePtr;

        /// Non-null when
        llvm::BasicBlock *landingPad;
    };

    /// The SRInfo object associated with the subroutine this frame is
    /// representing.
    SRInfo *SRI;

    /// IRBuilder used to generate the code for this subroutine.
    llvm::IRBuilder<> &Builder;

    /// A basic block holding all alloca'd data in this subroutine.
    llvm::BasicBlock *allocaBB;

    /// The final return basic block;
    llvm::BasicBlock *returnBB;

    /// Current subframe entry.
    Subframe *currentSubframe;

    /// The return value for this function.  This member is null when we are
    /// generating an procedure or function using the sret calling convention.
    llvm::Value *returnValue;

    /// The implicit instance parameter for this function.
    llvm::Value *implicitContext;

    // Map from Comma Decl's and Type's to AllocaEntry's.
    typedef llvm::DenseMap<const Ast*, ActivationEntry*> EntryMap;
    EntryMap entryTable;

    /// Initialize the implicit first parameter.  Also, name the llvm arguments
    /// after the source formals, and populate the lookup tables such that a
    /// search for a parameter decl yields the corresponding llvm value.
    void injectSubroutineArgs(CodeGenRoutine &CGR);
};

} // end comma namespace.

#endif
