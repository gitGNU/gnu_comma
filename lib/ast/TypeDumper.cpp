//===-- ast/TypeDumper.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "TypeDumper.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Type.h"

#include "llvm/Support/Format.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

llvm::raw_ostream &TypeDumper::printHeader(Type *node)
{
    AstDumperBase::printHeader(node);
    if (DiscreteType *discrete = dyn_cast<DiscreteType>(node))
        S << " '" << discrete->getIdInfo()->getString() << '\'';
    return S;
}

llvm::raw_ostream &TypeDumper::dump(Type *type, unsigned level)
{
    unsigned savedLevel = indentLevel;
    indentLevel = level;
    visitType(type);
    indentLevel = savedLevel;
    return S;
}

llvm::raw_ostream &TypeDumper::dumpAST(Ast *node)
{
    return dumper->dump(node, indentLevel);
}

llvm::raw_ostream &TypeDumper::dumpParameters(SubroutineType *node)
{
    unsigned arity = node->getArity();
    unsigned argIndex = 0;

    S << "(";
    while (argIndex < arity) {
        visitType(node->getArgType(argIndex));
        if (++argIndex < arity)
            S << "; ";
    }
    return S << ")";
}

void TypeDumper::visitDomainType(DomainType *node)
{
    printHeader(node);
    if (DomainInstanceDecl *instance = node->getInstanceDecl()) {
        indent();
        for (unsigned i = 0; i < instance->getArity(); ++i) {
            S << '\n';
            printIndentation();
            visitType(instance->getActualParamType(i));
        }
        dedent();
    }
    S << '>';
}

void TypeDumper::visitFunctionType(FunctionType *node)
{
    printHeader(node) << ' ';
    dumpParameters(node) << ' ';
    visitType(node->getReturnType());
    S << '>';
}

void TypeDumper::visitProcedureType(ProcedureType *node)
{
    printHeader(node) << ' ';
    dumpParameters(node) << '>';
}

void TypeDumper::visitEnumerationType(EnumerationType *node)
{
    printHeader(node) << '>';
}

void TypeDumper::visitIntegerType(IntegerType *node)
{
    printHeader(node);
    if (Range *range = node->getConstraint()) {
        dumpAST(range->getLowerBound());
        S << ' ';
        dumpAST(range->getUpperBound());
    }
    S << '>';
}

void TypeDumper::visitIncompleteType(IncompleteType *node)
{
    printHeader(node) << '>';
}

void TypeDumper::visitArrayType(ArrayType *node)
{
    printHeader(node) << '>';
}

void TypeDumper::visitAccessType(AccessType *node)
{
    printHeader(node) << ' ';
    visitType(node->getTargetType());
    S << '>';
}
