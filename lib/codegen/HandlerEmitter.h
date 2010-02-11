//===-- codegen/HandlerEmitter.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_HANDLEREMITTER_HDR_GUARD
#define COMMA_CODEGEN_HANDLEREMITTER_HDR_GUARD

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file declares the HandlerEmitter class.
//===----------------------------------------------------------------------===//

namespace llvm {

class BasicBlock;

} // end llvm namespace.

namespace comma {

class CodeGenRoutine;
class Frame;
class StmtSequence;

//===----------------------------------------------------------------------===//
// Handler
//
/// \class
///
/// The HandlerEmitter class provides routines to emit the exception handlers
/// associated with a handled sequence of statements.
class HandlerEmitter {

public:
    HandlerEmitter(CodeGenRoutine &CGR);

    /// Emits the exception handlers associated with the given StmtSequence.
    ///
    /// The given statement sequence must provide at least one handler.  The
    /// current frame must provide a landing pad.  Emits the handler code into
    /// the landing pad and unifies the control flow to target \p mergeBB if
    /// non-null.  If \p mergeBB is null, an implicit basic block is generated.
    /// The insertion point of the builder is set to the merge block on return.
    void emitHandlers(StmtSequence *seq, llvm::BasicBlock *mergeBB = 0);

private:
    CodeGenRoutine &CGR;
    CodeGen &CG;
    CommaRT &RT;

    /// Provides access to the current frame.
    Frame *frame();

    /// Emits a call to llvm.eh.selector using the given exception object and
    /// which covers all of the exception occurrences handled by the given
    /// sequence.  Returns the resulting type info index.
    llvm::Value *emitSelector(llvm::Value *exception, StmtSequence *sequence);
};

} // end comma namespace.

#endif
