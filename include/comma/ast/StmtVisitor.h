//===-- ast/StmtVisitor.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file provides a virtual class used for implementing the vistor
/// pattern across statement nodes.
///
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_STMTVISITOR_HDR_GUARD
#define COMMA_AST_SRMTVISITOR_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

class StmtVisitor {

public:
    virtual ~StmtVisitor() { }

    /// \name Inner Visitor Methods.
    ///
    /// The following set of methods are concerned with visiting the inner nodes
    /// of the statement hierarchy.  Default implementations are provided.  The
    /// behaviour is to simply dispatch over the set of concrete subclasses.
    /// For example, the default method for visiting a StmtSequence will resolve
    /// its argument to a BlockStmt and then invoke the specialized visitor for
    /// the resolved type.  Of course, an implementation may choose to override
    /// any or all of these methods.
    ///
    ///@{
    virtual void visitAst(Ast *node);
    virtual void visitStmt(Stmt *node);
    virtual void visitStmtSequence(StmtSequence *node);
    ///@}

    /// \name Concrete Visitor Methods.
    ///
    /// The following group of methods visit the non-virtual nodes in the type
    /// hierarchy.  The default implementation for these methods do nothing.
    ///
    ///@{
    virtual void visitBlockStmt(BlockStmt *node);
    virtual void visitProcedureCallStmt(ProcedureCallStmt *node);
    virtual void visitReturnStmt(ReturnStmt *node);
    virtual void visitAssignmentStmt(AssignmentStmt *node);
    virtual void visitIfStmt(IfStmt *node);
    virtual void visitWhileStmt(WhileStmt *node);
    virtual void visitForStmt(ForStmt *node);
    virtual void visitLoopStmt(LoopStmt *node);
    virtual void visitRaiseStmt(RaiseStmt *node);
    virtual void visitPragmaStmt(PragmaStmt *node);
    virtual void visitNullStmt(NullStmt *node);
    ///@}
};

} // end comma namespace.

#endif

