//===-- ast/TypeDumper.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief A class which provides facilities for dumping details about
/// type nodes.
///
/// The facilities provided by this class are useful for debugging purposes and
/// are used by the implementation of Ast::dump().
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_TYPEDUMPER_HDR_GUARD
#define COMMA_AST_TYPEDUMPER_HDR_GUARD

#include "AstDumper.h"
#include "comma/ast/TypeVisitor.h"

#include "llvm/Support/raw_ostream.h"

namespace comma {

class TypeDumper : public AstDumperBase, private TypeVisitor {

public:
    /// Constructs a dumper which writes its output to the given
    /// llvm::raw_ostream.
    TypeDumper(llvm::raw_ostream &stream, AstDumper *dumper)
        : AstDumperBase(stream), dumper(dumper) { }

    /// Dumps the given type node to the stream respecting the given indentation
    /// level.
    llvm::raw_ostream &dump(Type *type, unsigned level = 0);

private:
    /// Generic dumper for printing non-stmt nodes.
    AstDumper *dumper;

    /// Helper method for the printing of subroutine parameters.  Prints the
    /// list of paremeters for the type delimited by "(" and ")".  No trailing
    /// space.
    llvm::raw_ostream &dumpParameters(SubroutineType *node);

    /// Specialization for types.
    llvm::raw_ostream &printHeader(Type *node);

    /// Dumps the given node maintaining the current indentation level.
    llvm::raw_ostream &dumpAST(Ast *node);

    /// Visitor methods implementing the dump routines.  We use the default
    /// implementations for all inner node visitors since we are only concerned
    /// with concrete types.
    ///
    /// Conventions: The visitors begin their printing directly to the stream.
    /// They never start a new line or indent the stream.  Furthermore, they
    /// never terminate their output with a new line.  As all printed objects
    /// are delimited with '<' and '>', the last character printed is always
    /// '>'.  The indentation level can change while a node is being printed,
    /// but the level is always restored once the printing is complete.
    void visitFunctionType(FunctionType *node);
    void visitProcedureType(ProcedureType *node);
    void visitEnumerationType(EnumerationType *node);
    void visitIncompleteType(IncompleteType *node);
    void visitIntegerType(IntegerType *node);
    void visitArrayType(ArrayType *node);
    void visitAccessType(AccessType *node);
    void visitRecordType(RecordType *node);
    void visitPrivateType(PrivateType *node);
};

} // end comma namespace.

#endif

