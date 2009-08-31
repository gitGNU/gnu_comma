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
    if (NamedType *namedTy = dyn_cast<NamedType>(node))
        visitNamedType(namedTy);
    else if (SubroutineType *srTy = dyn_cast<SubroutineType>(node))
        visitSubroutineType(srTy);
    else if (IntegerType *intTy = dyn_cast<IntegerType>(node))
        visitIntegerType(intTy);
    else
        assert(false && "Cannot visit this kind of node!");
}

void TypeVisitor::visitNamedType(NamedType *node)
{
    if (CarrierType *carrierTy = dyn_cast<CarrierType>(node))
        visitCarrierType(carrierTy);
    else if (SignatureType *sigTy = dyn_cast<SignatureType>(node))
        visitSignatureType(sigTy);
    else if (DomainType *domTy = dyn_cast<DomainType>(node))
        visitDomainType(domTy);
    else if (EnumerationType *enumTy = dyn_cast<EnumerationType>(node))
        visitEnumerationType(enumTy);
    else if (TypedefType *tydefTy = dyn_cast<TypedefType>(node))
        visitTypedefType(tydefTy);
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
void TypeVisitor::visitSignatureType(SignatureType *node) { }
void TypeVisitor::visitDomainType(DomainType *node) { }
void TypeVisitor::visitFunctionType(FunctionType *node) { }
void TypeVisitor::visitProcedureType(ProcedureType *node) { }
void TypeVisitor::visitEnumerationType(EnumerationType *node) { }
void TypeVisitor::visitIntegerType(IntegerType *node) { }
void TypeVisitor::visitTypedefType(TypedefType *node) { }

