//===-- ast/DeclVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
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
    if (SubroutineDecl *routine = dyn_cast<SubroutineDecl>(node))
        visitSubroutineDecl(routine);
    else if (TypeDecl *typed = dyn_cast<TypeDecl>(node))
        visitTypeDecl(typed);
    else if (ValueDecl *value = dyn_cast<ValueDecl>(node))
        visitValueDecl(value);
    else if (ExceptionDecl *exception = dyn_cast<ExceptionDecl>(node))
        visitExceptionDecl(exception);
    else if (PackageDecl *package = dyn_cast<PackageDecl>(node))
        visitPackageDecl(package);
    else if (ComponentDecl *component = dyn_cast<ComponentDecl>(node))
        visitComponentDecl(component);
    else
        assert(false && "Cannot visit this kind of node!");
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
    case DISPATCH(ArrayDecl, node);
    case DISPATCH(EnumerationDecl, node);
    case DISPATCH(IntegerDecl, node);
    case DISPATCH(IncompleteTypeDecl, node);
    case DISPATCH(PrivateTypeDecl, node);
    case DISPATCH(AccessDecl, node);
    case DISPATCH(RecordDecl, node);
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
    case DISPATCH(RenamedObjectDecl, node);
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

void DeclVisitor::visitUseDecl(UseDecl *node) { }
void DeclVisitor::visitBodyDecl(BodyDecl *node) { }
void DeclVisitor::visitPackageDecl(PackageDecl *node) { }
void DeclVisitor::visitPkgInstanceDecl(PkgInstanceDecl *node) { }
void DeclVisitor::visitProcedureDecl(ProcedureDecl *node) { }
void DeclVisitor::visitLoopDecl(LoopDecl *node) { }
void DeclVisitor::visitParamValueDecl(ParamValueDecl *node) { }
void DeclVisitor::visitObjectDecl(ObjectDecl *node) { }
void DeclVisitor::visitRenamedObjectDecl(RenamedObjectDecl *node) { }
void DeclVisitor::visitEnumLiteral(EnumLiteral *node) { }
void DeclVisitor::visitEnumerationDecl(EnumerationDecl *node) { }
void DeclVisitor::visitIntegerDecl(IntegerDecl *node) { }
void DeclVisitor::visitArrayDecl(ArrayDecl *node) { }
void DeclVisitor::visitExceptionDecl(ExceptionDecl *node) { }
void DeclVisitor::visitIncompleteTypeDecl(IncompleteTypeDecl *node) { }
void DeclVisitor::visitPrivateTypeDecl(PrivateTypeDecl *node) { }
void DeclVisitor::visitAccessDecl(AccessDecl *node) { }
void DeclVisitor::visitRecordDecl(RecordDecl *node) { }
void DeclVisitor::visitComponentDecl(ComponentDecl *node) { }
