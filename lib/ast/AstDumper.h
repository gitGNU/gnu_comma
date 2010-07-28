//===-- ast/AstDumper.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Facilities for dumping details about ast nodes.
///
/// The facilities provided by this class are useful for debugging purposes and
/// are used by the implementation of Ast::dump().
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTDUMPER_HDR_GUARD
#define COMMA_AST_ASTDUMPER_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/basic/ParameterModes.h"

#include "llvm/Support/raw_ostream.h"

namespace comma {

/// A virtual base class encapsulating common functionality amongst the various
/// dumpers.
class AstDumperBase {

public:
    virtual ~AstDumperBase() { }

    /// Constructs a dumper which prints to the given stream.
    AstDumperBase(llvm::raw_ostream &S) : S(S), indentLevel(0) { }

protected:
    /// The stream we are writing to.
    llvm::raw_ostream &S;

    /// The current indentation level.
    unsigned indentLevel;

    /// Increments the indentation level.  Typically, this is paired with a
    /// corresponding call to dedent().
    void indent() { indentLevel += 2; }

    /// Decrements the indentation level.
    void dedent() { indentLevel -= 2; }

    /// Prints a common header to the stream of the form "<kind-name
    /// pointer-value".  Note the lack of a trailing space.  This method can be
    /// overriden in subclasses to implement their own notion of a common
    /// header.
    virtual llvm::raw_ostream &printHeader(Ast *node);

    /// Emits this->indentLevel many spaces onto the output stream.
    ///
    /// FIXME:  This method can be removed once llvm-2.7 is released.
    /// raw_ostream should have its own indent method at that time.
    llvm::raw_ostream &printIndentation();

    /// Prints a code indicating the given parameter mode to the stream.  Either
    /// "I", "O", "IO", or "D" is printed (no trailing space), meaning "in"
    /// "out" "in out" or default, respecively.
    llvm::raw_ostream &dumpParamMode(PM::ParameterMode mode);
};

/// Forward declarations for the various types of dumpers.
class DeclDumper;
class ExprDumper;
class StmtDumper;
class TypeDumper;

class AstDumper : public AstDumperBase {

public:
    /// Constructs a dumper which writes its output to the given
    /// llvm::raw_ostream.
    AstDumper(llvm::raw_ostream &stream);

    ~AstDumper();

    /// Dumps the given node to the output stream respecting the given
    /// indentation level;
    llvm::raw_ostream &dump(Ast *node, unsigned level = 0);

private:
    /// Specific dumpers responsible for printing specialized sub-graphs of the
    /// ast hierarchy.
    DeclDumper *DDumper;
    ExprDumper *EDumper;
    StmtDumper *SDumper;
    TypeDumper *TDumper;

    /// Dumps the given decl thru the DeclDumper, forwarding the current
    /// indentation level.
    llvm::raw_ostream &dumpDecl(Decl *node);

    /// Dumps the given expression thru the ExprDumper, forwarding the current
    /// indentation level.
    llvm::raw_ostream &dumpExpr(Expr *node);

    /// Dumps the given statement thru the StmtDumper, forwarding the current
    /// indentation level.
    llvm::raw_ostream &dumpStmt(Stmt *node);

    /// Dumps the given type thru the TypeDumper, forwarding the current
    /// indentation level.
    llvm::raw_ostream &dumpType(Type *node);

    /// Dumps a range attribute.
    llvm::raw_ostream &dumpRangeAttrib(RangeAttrib *node);

    /// Dumps a DSTDefinition node.
    llvm::raw_ostream &dumpDSTDefinition(DSTDefinition *node);
};

} // end comma namespace.

#endif
