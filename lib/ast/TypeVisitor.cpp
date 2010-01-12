//===-- ast/TypeVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Type.h"
#include "comma/ast/TypeVisitor.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Virtual visitors.  These methods provide implementations which walk the type
// hierarchy graph.

void TypeVisitor::visitType(Type *node)
{
    if (SubroutineType *srTy = dyn_cast<SubroutineType>(node))
        visitSubroutineType(srTy);
    else if (DomainType *domTy = dyn_cast<DomainType>(node))
        visitDomainType(domTy);
    else if (IntegerType *intTy = dyn_cast<IntegerType>(node))
        visitIntegerType(intTy);
    else if (ArrayType *arrTy = dyn_cast<ArrayType>(node))
        visitArrayType(arrTy);
    else if (EnumerationType *enumTy = dyn_cast<EnumerationType>(node))
        visitEnumerationType(enumTy);
    else if (RecordType *recTy = dyn_cast<RecordType>(node))
        visitRecordType(recTy);
    else if (AccessType *accessTy = dyn_cast<AccessType>(node))
        visitAccessType(accessTy);
    else if (IncompleteType *incompleteTy = dyn_cast<IncompleteType>(node))
        visitIncompleteType(incompleteTy);
    else
        assert(false && "Cannot visit this kind of node!");
}

void TypeVisitor::visitSubroutineType(SubroutineType *node)
{
    if (FunctionType *fnTy = dyn_cast<FunctionType>(node))
        visitFunctionType(fnTy);
    else if (ProcedureType *procTy = dyn_cast<ProcedureType>(node))
        visitProcedureType(procTy);
    else
        assert(false && "Cannot visit this kind of node!");
}

//===----------------------------------------------------------------------===//
// Non-virtual visitors, with empty out-of-line implementations.

void TypeVisitor::visitDomainType(DomainType *node) { }
void TypeVisitor::visitFunctionType(FunctionType *node) { }
void TypeVisitor::visitProcedureType(ProcedureType *node) { }
void TypeVisitor::visitEnumerationType(EnumerationType *node) { }
void TypeVisitor::visitIntegerType(IntegerType *node) { }
void TypeVisitor::visitArrayType(ArrayType *node) { }
void TypeVisitor::visitRecordType(RecordType *node) { }
void TypeVisitor::visitAccessType(AccessType *node) { }
void TypeVisitor::visitIncompleteType(IncompleteType *node) { }
