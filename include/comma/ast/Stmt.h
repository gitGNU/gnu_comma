//===-- ast/Stmt.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstBase.h"
#include "llvm/ADT/SmallVector.h"

#ifndef COMMA_AST_STMT_HDR_GUARD
#define COMMA_AST_STMT_HDR_GUARD

namespace comma {

//===----------------------------------------------------------------------===//
// Stmt
class Stmt : public Ast {

protected:
    Stmt(AstKind kind) : Ast(kind) { }

    static bool classof(const Stmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesStmt();
    }
};

//===----------------------------------------------------------------------===//
// StmtSequence
//
// Represents a sequence of statements.
class StmtSequence : public Stmt {

    llvm::SmallVector<Stmt*, 16> statements;

protected:
    StmtSequence(AstKind kind) : Stmt(kind) { }

public:
    void addStmt(Stmt *stmt) { stmtSequence.push_back(stmt); }

    typedef llvm::SmallVector<Stmt*, 16>::iterator StmtIter;
    StmtIter beginStatements() { return statements.begin(); }
    StmtIter endStatements()   { return statements.end(); }

    typedef llvm::SmallVector<Stmt*, 16>::const_iterator ConstStmtIter;
    ConstStmtIter beginStatements() const { return statements.begin(); }
    ConstStmtIter endStatements()   const { return statements.end(); }

    static bool classof(const StmtSequence *node) { return true; }

private:
    IdentifierInfo *label;
};


//===----------------------------------------------------------------------===//
// BlockStmt
//
// Represents a block statement consisting of an optional identifier, a possibly
// empty declarative region, and a sequence of statements constituting the body.
class BlockStmt : public StmtSequence, public DeclarativeRegion {

public:
    BlockStmt() : StmtSequence(AST_BlockStmt), label(0) { }

    BlockStmt(IdentifierInfo *label) : StmtSequence(AST_BlockStmt), label(label) { }

    IdentifierInfo *getLabel() { return label; }

    static bool classof(const BlockStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_BlockStmt;
    }

private:
    IdentifierInfo *label;
};

} // End comma namespace.

#endif
