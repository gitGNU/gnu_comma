//===-- ast/StmtDumper.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief A class which provides facilities for dumping details about
/// statement nodes.
///
/// The facilities provided by this class are useful for debugging purposes and
/// are used by the implementation of Ast::dump().
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_STMTDUMPER_HDR_GUARD
#define COMMA_AST_STMTDUMPER_HDR_GUARD

#include "AstDumper.h"
#include "DeclDumper.h"
#include "ExprDumper.h"
#include "comma/ast/StmtVisitor.h"

namespace comma {

class StmtDumper : public AstDumperBase, private StmtVisitor {

public:
    /// Constructs a dumper which writes its output to the given
    /// llvm::raw_ostream.
    StmtDumper(llvm::raw_ostream &stream, AstDumper *dumper)
        : AstDumperBase(stream), dumper(dumper) { }

    /// Dumps the given statement node to the output stream respecting the given
    /// indentation level.
    llvm::raw_ostream &dump(Stmt *stmt, unsigned level = 0);

private:
    /// Generic dumper for printing non-stmt nodes.
    AstDumper *dumper;

    /// Dumps the given node maintaining the current indentation level.
    llvm::raw_ostream &dumpAST(Ast *node);

    /// Visitor methods implementing the dump routines.  We use the default
    /// implementations for all inner node visitors since we are only concerned
    /// with concrete statement nodes.
    ///
    /// Conventions: The visitors begin their printing directly to the stream.
    /// They never start a new line or indent the stream.  Furthermore, they
    /// never terminate their output with a new line.  As all printed objects
    /// are delimited with '<' and '>', the last character printed is always
    /// '>'.  The indentation level can change while a node is being printed,
    /// but the level is always restored once the printing is complete.
    void visitStmtSequence(StmtSequence *node);
    void visitBlockStmt(BlockStmt *node);
    void visitProcedureCallStmt(ProcedureCallStmt *node);
    void visitReturnStmt(ReturnStmt *node);
    void visitAssignmentStmt(AssignmentStmt *node);
    void visitIfStmt(IfStmt *node);
    void visitWhileStmt(WhileStmt *node);
    void visitForStmt(ForStmt *node);
    void visitLoopStmt(LoopStmt *node);
    void visitPragmaStmt(PragmaStmt *node);
};

} // end comma namespace.

#endif
