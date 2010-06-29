//===-- ast/TypeVisitor.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===---------------------------------------------------------------------===//
/// \file
///
/// \brief This file provides a virtual class used for implementing the visitor
/// parrern accross type nodes.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_TYPEVISITOR_HDR_GUARD
#define COMMA_AST_TYPEVISITOR_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

class TypeVisitor {

public:
    virtual ~TypeVisitor() { }

    /// \name Inner Visitor Methods.
    ///
    /// The following set of methods are concerned with visiting the inner nodes
    /// of the type hierarchy.  Default implementations are provided.  The
    /// behaviour is to simply dispatch over the set of concrete subclasses.
    /// For example, the default method for visiting a SubroutineType will
    /// resolve its argument to either a FunctionType or ProcedureType and then
    /// invoke the specialized visitor for the resolved type.  Of course, an
    /// implementation may choose to override any or all of these methods.
    ///
    //@{
    virtual void visitType(Type *node);
    virtual void visitSubroutineType(SubroutineType *node);
    //@}

    /// \name Concrete Visitor Methods.
    ///
    /// The following group of methods visit the non-virtual nodes in the type
    /// hierarchy.  The default implementation for these methods do nothing.
    ///
    //@{
    virtual void visitFunctionType(FunctionType *node);
    virtual void visitProcedureType(ProcedureType *node);
    virtual void visitEnumerationType(EnumerationType *node);
    virtual void visitIntegerType(IntegerType *node);
    virtual void visitArrayType(ArrayType *node);
    virtual void visitRecordType(RecordType *node);
    virtual void visitAccessType(AccessType *node);
    virtual void visitIncompleteType(IncompleteType *node);
    //@}
};

} // end comma namespace.

#endif
