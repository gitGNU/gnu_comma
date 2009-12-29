//===-- ast/DiagPrint.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/DeclVisitor.h"
#include "comma/ast/Expr.h"
#include "comma/ast/ExprVisitor.h"
#include "comma/ast/DiagPrint.h"
#include "comma/ast/TypeVisitor.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

//===----------------------------------------------------------------------===//
// PrettyPrinter.
//
/// \class
///
/// \brief Pretty prints AST nodes by subclassing the visitor classes.
class PrettyPrinter : private TypeVisitor,
                      private DeclVisitor {

public:
    PrettyPrinter(llvm::raw_ostream &stream) : stream(stream) { }

    void print(Type *type) { visitType(type); }
    void print(Decl *decl) { visitDecl(decl); }

private:
    llvm::raw_ostream &stream;

    // Type printers.
    void visitDomainType(DomainType *node);
    void visitSubroutineType(SubroutineType *node);
    void visitEnumerationType(EnumerationType *node);
    void visitIntegerType(IntegerType *node);
    void visitArrayType(ArrayType *node);
    void visitRecordType(RecordType *node);

    // Decl printers.  Currently only a few are supported.
    void visitFunctionDecl(FunctionDecl *node);
    void visitProcedureDecl(ProcedureDecl *node);
    void visitDomainTypeDecl(DomainTypeDecl *node);

    // Helper methods.
    void printParameterProfile(SubroutineDecl *node);
    void printQualifiedName(const char *name, DeclRegion *region);
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// PrettyPrinter methods.

void PrettyPrinter::visitDomainType(DomainType *node)
{
    visitDomainTypeDecl(node->getDomainTypeDecl());
}

void PrettyPrinter::visitSubroutineType(SubroutineType *node)
{
    if (isa<FunctionType>(node))
        stream << "function";
    else
        stream << "procedure";

    if (node->getArity() != 0) {
        stream << '(';
        SubroutineType::arg_type_iterator I = node->begin();
        visitType(*I);
        while (++I != node->end()) {
            stream << ", ";
            visitType(*I);
        }
        stream << ')';
    }

    if (isa<FunctionType>(node)) {
        stream << " return ";
        visitType(cast<FunctionType>(node)->getReturnType());
    }
}

void PrettyPrinter::visitEnumerationType(EnumerationType *node)
{
    stream << node->getIdInfo()->getString();
}

void PrettyPrinter::visitIntegerType(IntegerType *node)
{
    stream << node->getIdInfo()->getString();
}

void PrettyPrinter::visitArrayType(ArrayType *node)
{
    stream << node->getIdInfo()->getString();
}

void PrettyPrinter::visitRecordType(RecordType *node)
{
    stream << node->getIdInfo()->getString();
}

void PrettyPrinter::printParameterProfile(SubroutineDecl *node)
{
    unsigned arity = node->getArity();

    if (arity == 0)
        return;

    stream << " (";
    for (unsigned i = 0; i < arity; ++i) {
        IdentifierInfo *key = node->getParamKeyword(i);
        stream << key->getString() << ": ";

        switch (node->getParamMode(i)) {
        case PM::MODE_DEFAULT:
        case PM::MODE_IN:
            stream << "in ";
            break;
        case PM::MODE_OUT:
            stream << "out ";
                break;
        case PM::MODE_IN_OUT:
            stream << "in out ";
            break;
        }

        visitType(node->getParamType(i));

        if (i + 1 != arity)
            stream << "; ";
    }
    stream << ')';
}

void PrettyPrinter::printQualifiedName(const char *name, DeclRegion *region)
{
    typedef llvm::SmallVector<Ast*, 4> ParentSet;
    ParentSet parents;

    while (region) {
        // Bump past AddDecl's and add the containing PercentDecl.
        Ast *ast = region->asAst();
        if (isa<AddDecl>(ast)) {
            region = region->getParent();
            parents.push_back(region->asAst());
        }
        else
            parents.push_back(ast);
        region = region->getParent();
    }

    ParentSet::reverse_iterator I = parents.rbegin();
    ParentSet::reverse_iterator E = parents.rend();
    for (; I != E; ++I) {
        if (DomainTypeDecl *decl = dyn_cast<DomainTypeDecl>(*I))
            visitDomainTypeDecl(decl);
        else if (Decl *decl = dyn_cast<Decl>(*I))
            stream << decl->getString();
        stream << '.';
    }

    stream << name;
}

void PrettyPrinter::visitFunctionDecl(FunctionDecl *node)
{
    stream << "function ";
    printQualifiedName(node->getString(), node->getDeclRegion());
    printParameterProfile(node);
    stream << " return ";
    visitType(node->getReturnType());
}

void PrettyPrinter::visitProcedureDecl(ProcedureDecl *node)
{
    stream << "procedure ";
    printQualifiedName(node->getString(), node->getDeclRegion());
    printParameterProfile(node);
}

void PrettyPrinter::visitDomainTypeDecl(DomainTypeDecl *node)
{
    if (isa<PercentDecl>(node))
        stream << '%';
    else if (DomainInstanceDecl *decl = dyn_cast<DomainInstanceDecl>(node)) {
        stream << decl->getString();

        if (decl->isParameterized()) {
            stream << '(';
            DomainInstanceDecl::arg_iterator I = decl->beginArguments();
            visitDomainTypeDecl(*I);
            while (++I != decl->endArguments()) {
                stream << ", ";
                visitDomainTypeDecl(*I);
            }
            stream << ')';
        }
    }
    else if (AbstractDomainDecl *decl = dyn_cast<AbstractDomainDecl>(node))
        stream << decl->getString();
}


//===----------------------------------------------------------------------===//
// Public API.

void diag::PrintType::print(llvm::raw_ostream &stream) const
{
    PrettyPrinter(stream).print(type);
}

void diag::PrintDecl::print(llvm::raw_ostream &stream) const
{
    PrettyPrinter(stream).print(decl);
}
