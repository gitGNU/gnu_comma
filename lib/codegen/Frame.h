//===-- codegen/Frame.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
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

class Frame;

//===----------------------------------------------------------------------===//
class Subframe {

public:
    ~Subframe();

    /// Subframes of the following sorts can be constructed.
    enum Kind {
        Entry,                  ///< Entry point of a subroutine, tagged.
        Block,                  ///< Block frame, possibly tagged.
        Loop,                   ///< Loop frame, possibly tagged.
    };

    /// Returns the kind of this subframe.
    Kind getKind() const { return kind; }

    /// Returns the name of this subframe.
    ///
    /// The name of a subframe is defined to be the name (if any) associated
    /// with the associated entry basic block.
    const llvm::StringRef getName() const { return entryBB->getName(); }

    /// Returns true if this is a nested subframe.
    bool hasParent() const { return parent != 0; }

    /// Returns the parent of this frame, or null if hasParent() returns
    /// false.
    Subframe *getParent() { return parent; }

    /// Returns the landing pad basic block associated with this subframe as
    /// a result of a call to requireLandingPad, or null if
    /// requireLandingPad has yet to be called on this subframe.
    llvm::BasicBlock *getLandingPad() { return landingPad; }

    /// Returns the entry basic block associated with this frame.
    llvm::BasicBlock *getEntryBB() { return entryBB; }

    /// Returns the merge basic block associated with this frame.
    llvm::BasicBlock *getMergeBB() { return mergeBB; }

private:
    /// \name Interface for use by Frame.
    //@{
    Subframe(Kind kind, Frame *context, Subframe *parent,
             const llvm::Twine &name);
    Subframe(Kind kind, Frame *context, Subframe *parent,
             llvm::BasicBlock *entryBB, llvm::BasicBlock *mergeBB);

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
    //@}

    friend class Frame;

    /// The kind of this subframe.
    Kind kind;

    /// Back-link to the managing Frame.
    Frame *SRF;

    /// The Subframe which contains this one, or null if this is a top-level
    /// frame.
    Subframe *parent;

    /// Non-null when emitStacksave is called on this Subframe.  Holds the
    /// restoration pointer for use in a stackrestore call.
    llvm::Value *restorePtr;

    /// The landing pad associated with this subframe or null.
    llvm::BasicBlock *landingPad;

    /// The entry block.
    llvm::BasicBlock *entryBB;

    /// The unified merge block.
    llvm::BasicBlock *mergeBB;

    Subframe(const Subframe &);             // Do not implement.
    Subframe &operator =(const Subframe &); // Likewise.
};

//===----------------------------------------------------------------------===//
// Frame
class Frame {

public:
    Frame(SRInfo *routineInfo,
          CodeGenRoutine &CGR, llvm::IRBuilder<> &Builder);

    ~Frame();

    /// Returns the SRInfo object this Frame represents.
    SRInfo *getSRInfo() { return SRI; }

    /// Returns the subroutine associated with this frame.
    SubroutineDecl *getDeclaration() { return SRI->getDeclaration(); }

    /// Returns the LLVM function managed by this frame.
    llvm::Function *getLLVMFunction() { return SRI->getLLVMFunction(); }

    /// Returns the IRBuilder which should be used to emit code for this
    /// subroutine.
    llvm::IRBuilder<> &getIRBuilder() { return Builder; }

    /// Creates a basic block for this frame.
    llvm::BasicBlock *makeBasicBlock(const llvm::Twine &name = "",
                                     llvm::BasicBlock *insertBefore = 0);

    /// \name Subframe Methods.
    ///
    //@{

    /// Returns the currently active subframe.
    Subframe *subframe() { return currentSubframe; }

    /// Pushes a new subframe of the given kind and name and makes it current.
    /// Returns a basic block to be used as entry into this frame.  An exit
    /// basic block is generated and accesable thru Subframe::getMergeBB().
    llvm::BasicBlock *pushFrame(Subframe::Kind kind,
                                const llvm::Twine &name = "");

    /// Pushes a new subframe of the given kind and name and makes it current.
    /// Uses the given basic blocks as entry and exit points for the subframe.
    void pushFrame(Subframe::Kind kind,
                   llvm::BasicBlock *entryBB,
                   llvm::BasicBlock *mergeBB = 0);

    /// Pop's the current subframe.
    void popFrame();

    /// Marks the current subframe as a stacksave frame.
    void stacksave();

    /// Adds a landing pad to the current subframe.
    void addLandingPad();

    /// Returns true if there exists a subframe with an active landing pad.
    bool hasLandingPad();

    /// Returns the basic block for use as a landing pad, or null if
    /// hasLandingPad returns false.
    llvm::BasicBlock *getLandingPad();

    /// Removes the innermost landing pad from the subframe stack, if any.
    void removeLandingPad();

    /// Returns the innermost subframe of the given kind, or null no such
    /// subframe exists.
    Subframe *findFirstSubframe(Subframe::Kind kind);

    //@{
    /// Locates the innermost subframe with the given name, or null if no such
    /// subframe exists.
    Subframe *findFirstNamedSubframe(IdentifierInfo *name) {
        return findFirstNamedSubframe(name->getString());
    }
    Subframe *findFirstNamedSubframe(const llvm::Twine &name);
    //@}

    //@}

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

    void emitReturn();

    llvm::Value *getReturnValue() { return returnValue; }

    void emitPrologue(llvm::BasicBlock *bodyBB);
    void emitEpilogue();

private:
    /// The following structure is used to hold information about alloca'd
    /// objects within an Frame.
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
