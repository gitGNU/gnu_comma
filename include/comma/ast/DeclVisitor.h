//===-- ast/DeclVisitor.h ------------------------------------- -*- C++ -*-===//
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
/// pattern across declaration nodes.
///
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECLVISITOR_HDR_GUARD
#define COMMA_AST_DECLVISITOR_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

class DeclVisitor {

public:
    virtual ~DeclVisitor() { }

    /// \name Inner Visitor Methods.
    ///
    /// The AST hierarchy is a tree.  Multiple inheritance, when it is used,
    /// provides mixins which are outside of the hierarchy proper (for example
    /// DeclRegion).  The following set of methods are concerned with visiting
    /// the inner nodes of the declaration hierarchy.  Default implementations
    /// are provided.  The behaviour is to simply dispatch over the set of
    /// concrete subclasses.  For example, the default method for visiting a
    /// SubroutineDecl will resolve its argument to either a ProcedureDecl or
    /// FunctionDecl and then invoke the specialized visitor for the resolved
    /// type.  Of course, an implementation may choose to override any or all of
    /// these methods.
    ///
    ///@{
    virtual void visitAst(Ast *node);
    virtual void visitDecl(Decl *node);
    virtual void visitSubroutineDecl(SubroutineDecl *node);
    virtual void visitTypeDecl(TypeDecl *node);
    virtual void visitValueDecl(ValueDecl *node);
    ///@}

    /// \name Concrete Visitor Methods.
    ///
    /// The following group of methods visit the non-virtual nodes in the
    /// declaration hierarchy.  Note that these nodes are not necessarily leafs
    /// (for example, EnumLiteral is also a FunctionDecl).  The default
    /// implementations provided behave depending on the argument node being a
    /// leaf.  For leaf nodes, the default is to do nothing but return.  For
    /// non-leaf nodes, the default dispatches over the next level in the
    /// hierarchy and invokes the next most specific method.
    ///
    ///@{
    virtual void visitUseDecl(UseDecl *node);
    virtual void visitBodyDecl(BodyDecl *node);
    virtual void visitPackageDecl(PackageDecl *node);
    virtual void visitPkgInstanceDecl(PkgInstanceDecl *node);
    virtual void visitFunctionDecl(FunctionDecl *node);
    virtual void visitProcedureDecl(ProcedureDecl *node);
    virtual void visitLoopDecl(LoopDecl *node);
    virtual void visitParamValueDecl(ParamValueDecl *node);
    virtual void visitObjectDecl(ObjectDecl *node);
    virtual void visitRenamedObjectDecl(RenamedObjectDecl *node);
    virtual void visitEnumLiteral(EnumLiteral *node);
    virtual void visitEnumerationDecl(EnumerationDecl *node);
    virtual void visitIntegerDecl(IntegerDecl *node);
    virtual void visitArrayDecl(ArrayDecl *node);
    virtual void visitExceptionDecl(ExceptionDecl *node);
    virtual void visitIncompleteTypeDecl(IncompleteTypeDecl *node);
    virtual void visitPrivateTypeDecl(PrivateTypeDecl *node);
    virtual void visitAccessDecl(AccessDecl *node);
    virtual void visitRecordDecl(RecordDecl *node);
    virtual void visitComponentDecl(ComponentDecl *node);
    ///@}
};

} // end comma namespace.

#endif
