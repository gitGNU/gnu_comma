//===-- ast/DeclVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/DeclVisitor.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

/// Macro to help form switch dispatch tables.  Note that this macro will only
/// work with concrete node (those with a definite kind).
#define DISPATCH(TYPE, NODE)         \
    Ast::AST_ ## TYPE:               \
    visit ## TYPE(cast<TYPE>(NODE)); \
    break

//===----------------------------------------------------------------------===//
// Virtual inner node visitors.
//===----------------------------------------------------------------------===//

void DeclVisitor::visitAst(Ast *node)
{
    if (Decl *decl = dyn_cast<Decl>(node))
        visitDecl(decl);
}

void DeclVisitor::visitDecl(Decl *node)
{
    if (ModelDecl *model = dyn_cast<ModelDecl>(node))
        visitModelDecl(model);
    else if (SubroutineDecl *routine = dyn_cast<SubroutineDecl>(node))
        visitSubroutineDecl(routine);
    else if (TypeDecl *typed = dyn_cast<TypeDecl>(node))
        visitTypeDecl(typed);
    else if (ValueDecl *value = dyn_cast<ValueDecl>(node))
        visitValueDecl(value);
    else if (SigInstanceDecl *instance = dyn_cast<SigInstanceDecl>(node))
        visitSigInstanceDecl(instance);
    else
        assert(false && "Cannot visit this kind of node!");
}

void DeclVisitor::visitModelDecl(ModelDecl *node)
{
    if (Domoid *domoid = dyn_cast<Domoid>(node))
        visitDomoid(domoid);
    else if (Sigoid *sigoid = dyn_cast<Sigoid>(node))
        visitSigoid(sigoid);
    else
        assert(false && "Cannot visit this kind of node!");
}

void DeclVisitor::visitSigoid(Sigoid *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(SignatureDecl, node);
    case DISPATCH(VarietyDecl, node);
    };
}

void DeclVisitor::visitDomoid(Domoid *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(DomainDecl, node);
    case DISPATCH(FunctorDecl, node);
    };
}

void DeclVisitor::visitSubroutineDecl(SubroutineDecl *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(FunctionDecl, node);
    case DISPATCH(ProcedureDecl, node);
    };
}

void DeclVisitor::visitTypeDecl(TypeDecl *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(AbstractDomainDecl, node);
    case DISPATCH(ArrayDecl, node);
    case DISPATCH(ArraySubtypeDecl, node);
    case DISPATCH(CarrierDecl, node);
    case DISPATCH(DomainInstanceDecl, node);
    case DISPATCH(EnumerationDecl, node);
    case DISPATCH(EnumSubtypeDecl, node);
    case DISPATCH(IntegerDecl, node);
    case DISPATCH(IntegerSubtypeDecl, node);
    case DISPATCH(PercentDecl, node);
    };
}

void DeclVisitor::visitValueDecl(ValueDecl *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(ObjectDecl, node);
    case DISPATCH(ParamValueDecl, node);
    case DISPATCH(LoopDecl, node);
    };
}

void DeclVisitor::visitDomainTypeDecl(DomainTypeDecl *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(DomainInstanceDecl, node);
    case DISPATCH(PercentDecl, node);
    case DISPATCH(AbstractDomainDecl, node);
    };
}

//===----------------------------------------------------------------------===//
// Concrete inner node visitors.
//===----------------------------------------------------------------------===//

void DeclVisitor::visitFunctionDecl(FunctionDecl *node)
{
    if (EnumLiteral *enumLit = dyn_cast<EnumLiteral>(node))
        visitEnumLiteral(enumLit);
}

//===----------------------------------------------------------------------===//
// Leaf visitors.
//===----------------------------------------------------------------------===//

void DeclVisitor::visitImportDecl(ImportDecl *node) { }
void DeclVisitor::visitSignatureDecl(SignatureDecl *node) { }
void DeclVisitor::visitVarietyDecl(VarietyDecl *node) { }
void DeclVisitor::visitSigInstanceDecl(SigInstanceDecl *node) { }
void DeclVisitor::visitAddDecl(AddDecl *node) { }
void DeclVisitor::visitDomainDecl(DomainDecl *node) { }
void DeclVisitor::visitFunctorDecl(FunctorDecl *node) { }
void DeclVisitor::visitProcedureDecl(ProcedureDecl *node) { }
void DeclVisitor::visitCarrierDecl(CarrierDecl *node) { }
void DeclVisitor::visitAbstractDomainDecl(AbstractDomainDecl *node) { }
void DeclVisitor::visitDomainInstanceDecl(DomainInstanceDecl *node) { }
void DeclVisitor::visitPercentDecl(PercentDecl *node) { }
void DeclVisitor::visitLoopDecl(LoopDecl *node) { }
void DeclVisitor::visitParamValueDecl(ParamValueDecl *node) { }
void DeclVisitor::visitObjectDecl(ObjectDecl *node) { }
void DeclVisitor::visitEnumLiteral(EnumLiteral *node) { }
void DeclVisitor::visitEnumerationDecl(EnumerationDecl *node) { }
void DeclVisitor::visitEnumSubtypeDecl(EnumSubtypeDecl *node) { }
void DeclVisitor::visitIntegerDecl(IntegerDecl *node) { }
void DeclVisitor::visitIntegerSubtypeDecl(IntegerSubtypeDecl *node) { }
void DeclVisitor::visitArrayDecl(ArrayDecl *node) { }
void DeclVisitor::visitArraySubtypeDecl(ArraySubtypeDecl *node) { }
