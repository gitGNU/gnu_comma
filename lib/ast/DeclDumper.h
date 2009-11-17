//===-- ast/DeclDumper.h -------------------------------------- -*- C++ -*-===//
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
/// declaration nodes.
///
/// The facilities provided by this class are useful for debugging purposes and
/// are used by the implementation of Ast::dump().
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECLDUMPER_HDR_GUARD
#define COMMA_AST_DECLDUMPER_HDR_GUARD

#include "AstDumper.h"
#include "TypeDumper.h"
#include "StmtDumper.h"
#include "comma/ast/DeclVisitor.h"

#include "llvm/Support/raw_ostream.h"

namespace comma {

class DeclDumper : public AstDumperBase, private DeclVisitor {

public:
    /// Constructs a dumper which writes its output to the given
    /// llvm::raw_ostream.
    DeclDumper(llvm::raw_ostream &stream, AstDumper *dumper)
        : AstDumperBase(stream),
          dumper(dumper) { }

    /// Dumps the given decl node to the output stream respecting the given
    /// indentation level.
    llvm::raw_ostream &dump(Decl *decl, unsigned level = 0);

private:
    /// Generic dumper for handling non-expression nodes.
    AstDumper *dumper;

    /// Dumps to the generic dumper.
    llvm::raw_ostream &dumpAST(Ast *node) {
        return dumper->dump(node, indentLevel);
    }

    /// Override the supers implementation to ensure that the name of the decl
    /// is always printed.
    llvm::raw_ostream &printHeader(Ast *node);

    /// Conventions: The visitors begin their printing directly to the stream.
    /// They never start a new line or indent the stream.  Furthermore, they
    /// never terminate their output with a new line.  As all printed objects
    /// are delimited with '<' and '>', the last character printed is always
    /// '>'.  The indentation level can change while a node is being printed,
    /// but the level is always restored once the printing is complete.
    void visitImportDecl(ImportDecl *node);
    void visitSignatureDecl(SignatureDecl *node);
    void visitVarietyDecl(VarietyDecl *node);
    void visitSigInstanceDecl(SigInstanceDecl *node);
    void visitAddDecl(AddDecl *node);
    void visitDomainDecl(DomainDecl *node);
    void visitFunctorDecl(FunctorDecl *node);
    void visitSubroutineDecl(SubroutineDecl *node);
    void visitFunctionDecl(FunctionDecl *node);
    void visitProcedureDecl(ProcedureDecl *node);
    void visitCarrierDecl(CarrierDecl *node);
    void visitDomainTypeDecl(DomainTypeDecl *node);
    void visitAbstractDomainDecl(AbstractDomainDecl *node);
    void visitDomainInstanceDecl(DomainInstanceDecl *node);
    void visitParamValueDecl(ParamValueDecl *node);
    void visitObjectDecl(ObjectDecl *node);
    void visitEnumLiteral(EnumLiteral *node);
    void visitEnumerationDecl(EnumerationDecl *node);
    void visitEnumSubtypeDecl(EnumSubtypeDecl *node);
    void visitIntegerDecl(IntegerDecl *node);
    void visitIntegerSubtypeDecl(IntegerSubtypeDecl *node);
    void visitArrayDecl(ArrayDecl *node);
};

} // end comma namespace.

#endif
