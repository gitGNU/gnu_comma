//===-- ast/ExprDumper.h -------------------------------------- -*- C++ -*-===//
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
/// expression nodes.
///
/// The facilities provided by this class are useful for debugging purposes and
/// are used by the implementation of Ast::dump().
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_EXPRDUMPER_HDR_GUARD
#define COMMA_AST_EXPRDUMPER_HDR_GUARD

#include "AstDumper.h"
#include "comma/ast/ExprVisitor.h"

namespace comma {

class ExprDumper : public AstDumperBase, private ExprVisitor {

public:
    /// Constructs a dumper which writes its output to the given
    /// llvm::raw_ostream.
    ExprDumper(llvm::raw_ostream &stream, AstDumper *dumper)
        : AstDumperBase(stream),
          dumper(dumper) { }

    /// Dumps the given statement node to the output stream respecting the given
    /// indentation level.
    llvm::raw_ostream &dump(Expr *expr, unsigned level = 0);

private:
    /// Generic dumper for printing non-expression nodes.
    AstDumper *dumper;

    /// Prints to the generic dumper, forwarding the current indentation level.
    llvm::raw_ostream &dumpAST(Ast* node) {
        return dumper->dump(node, indentLevel);
    }

    /// Visitor methods implementing the dump routines.  We use the default
    /// implementations for all inner node visitors since we are only concerned
    /// with concrete expression nodes.
    ///
    /// Conventions: The visitors begin their printing directly to the stream.
    /// They never start a new line or indent the stream.  Furthermore, they
    /// never terminate their output with a new line.  As all printed objects
    /// are delimited with '<' and '>', the last character printed is always
    /// '>'.  The indentation level can change while a node is being printed,
    /// but the level is always restored once the printing is complete.
    void visitDeclRefExpr(DeclRefExpr *node);
    void visitFunctionCallExpr(FunctionCallExpr *node);
    void visitIndexedArrayExpr(IndexedArrayExpr *node);
    void visitInjExpr(InjExpr *node);
    void visitPrjExpr(PrjExpr *node);
    void visitIntegerLiteral(IntegerLiteral *node);
    void visitStringLiteral(StringLiteral *node);
    void visitPositionalAggExpr(PositionalAggExpr *node);
    void visitConversionExpr(ConversionExpr *node);
};

} // end comma namespace.

#endif
