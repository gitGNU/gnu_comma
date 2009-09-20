//===-- ast/TypeVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
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
    if (SubType *subTy = dyn_cast<SubType>(node))
        visitSubType(subTy);
    else if (SubroutineType *srTy = dyn_cast<SubroutineType>(node))
        visitSubroutineType(srTy);
    else if (DomainType *domTy = dyn_cast<DomainType>(node))
        visitDomainType(domTy);
    else if (IntegerType *intTy = dyn_cast<IntegerType>(node))
        visitIntegerType(intTy);
    else if (ArrayType *arrTy = dyn_cast<ArrayType>(node))
        visitArrayType(arrTy);
    else if (EnumerationType *enumTy = dyn_cast<EnumerationType>(node))
        visitEnumerationType(enumTy);
    else
        assert(false && "Cannot visit this kind of node!");
}

void TypeVisitor::visitSubType(SubType *node)
{
    if (CarrierType *carrierTy = dyn_cast<CarrierType>(node))
        visitCarrierType(carrierTy);
    else if (IntegerSubType *intTy = dyn_cast<IntegerSubType>(node))
        visitIntegerSubType(intTy);
    else if (ArraySubType *arrTy = dyn_cast<ArraySubType>(node))
        visitArraySubType(arrTy);
    else if (EnumSubType *enumTy = dyn_cast<EnumSubType>(node))
        visitEnumSubType(enumTy);
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

void TypeVisitor::visitCarrierType(CarrierType *node) { }
void TypeVisitor::visitDomainType(DomainType *node) { }
void TypeVisitor::visitFunctionType(FunctionType *node) { }
void TypeVisitor::visitProcedureType(ProcedureType *node) { }
void TypeVisitor::visitEnumerationType(EnumerationType *node) { }
void TypeVisitor::visitEnumSubType(EnumSubType *node) { }
void TypeVisitor::visitIntegerType(IntegerType *node) { }
void TypeVisitor::visitIntegerSubType(IntegerSubType *node) { }
void TypeVisitor::visitArrayType(ArrayType *node) { }
void TypeVisitor::visitArraySubType(ArraySubType *node) { }

