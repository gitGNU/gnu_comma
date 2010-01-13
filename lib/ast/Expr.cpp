//===-- ast/Expr.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"

#include <cstring>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Expr
//
// NOTE: Several methods related to compile time evaluation of expressions are
// defined in Eval.cpp.

bool Expr::isMutable(Expr *&immutable)
{
    // Iteration variable.  Updated as we walk subexpressions.
    Expr *cursor = this;

TRY_AGAIN:
    AstKind kind = cursor->getKind();

    // The base (and most common) case is that the cursor is a DeclRefExpr.
    // Either we have an object declaration or a formal parameter of mode "out"
    // or "in out".
    //
    // FIXME: We need to enhance this logic once constant declarations are
    // introduced.
    if (kind == AST_DeclRefExpr) {
        DeclRefExpr *ref = cast<DeclRefExpr>(cursor);
        ValueDecl *decl = ref->getDeclaration();
        bool result = true;

        // Object declarations are always mutable (currently).
        if (!isa<ObjectDecl>(decl)) {
            kind = decl->getKind();
            switch (kind) {

            case AST_ParamValueDecl: {
                ParamValueDecl *PVD = cast<ParamValueDecl>(decl);
                if (PVD->getParameterMode() == PM::MODE_IN) {
                    result = false;
                    immutable = cursor;
                }
                break;
            }

            case AST_RenamedObjectDecl: {
                // Recurse since renames are likely only one layer deep.
                RenamedObjectDecl *ROD = cast<RenamedObjectDecl>(decl);
                result = ROD->getRenamedExpr()->isMutable(immutable);
                break;
            }

            default:
                result = false;
                immutable = cursor;
                break;
            }
        }
        return result;
    }

    // Otherwise, attempt to walk thru the valid chain of subexpressions which
    // may ultimately yield a valid base case.  In essence, mutability is a
    // transitive property of the cursors prefix or operand.
    switch (kind) {

    default:
        // Nope.  Not mutable.
        immutable = cursor;
        return 0;

    case AST_SelectedExpr:
        cursor = cast<SelectedExpr>(cursor)->getPrefix();
        break;

    case AST_IndexedArrayExpr:
        cursor = cast<IndexedArrayExpr>(cursor)->getPrefix();
        break;

    case AST_DereferenceExpr:
        cursor = cast<DereferenceExpr>(cursor)->getPrefix();
        break;

    case AST_InjExpr:
        cursor = cast<InjExpr>(cursor)->getOperand();
        break;

    case AST_PrjExpr:
        cursor = cast<PrjExpr>(cursor)->getOperand();
        break;
    }

    // Continue to walk the expression tree and try again.
    goto TRY_AGAIN;
}

Expr *Expr::ignoreInjPrj()
{
    Expr *cursor = this;

    for (;;) {
        switch (cursor->getKind()) {

        default:
            return cursor;

        case Ast::AST_InjExpr:
            cursor = cast<InjExpr>(cursor)->getOperand();
            break;

        case Ast::AST_PrjExpr:
            cursor = cast<PrjExpr>(cursor)->getOperand();
            break;
        }
    }
}

//===----------------------------------------------------------------------===//
// FunctionCallExpr

FunctionCallExpr::FunctionCallExpr(SubroutineRef *connective,
                                   Expr **posArgs, unsigned numPos,
                                   KeywordSelector **keyArgs, unsigned numKeys)
    : Expr(AST_FunctionCallExpr, connective->getLocation()),
      SubroutineCall(connective, posArgs, numPos, keyArgs, numKeys)
{
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(SubroutineRef *connective)
    : Expr(AST_FunctionCallExpr, connective->getLocation()),
      SubroutineCall(connective, 0, 0, 0, 0)
{
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(FunctionDecl *connective, Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      SubroutineCall(connective, 0, 0, 0, 0)
{
    setTypeForConnective();
}

void FunctionCallExpr::setTypeForConnective()
{
    if (isUnambiguous()) {
        FunctionDecl *fdecl = getConnective();
        setType(fdecl->getReturnType());
    }
}

void FunctionCallExpr::resolveConnective(FunctionDecl *decl)
{
    SubroutineCall::resolveConnective(decl);
    setTypeForConnective();
}

//===----------------------------------------------------------------------===//
// IndexedArrayExpr

IndexedArrayExpr::IndexedArrayExpr(Expr *arrExpr,
                                   Expr **indices, unsigned numIndices)
    : Expr(AST_IndexedArrayExpr, arrExpr->getLocation()),
      indexedArray(arrExpr),
      numIndices(numIndices)
{
    assert(numIndices != 0 && "Missing indices!");

    if (arrExpr->hasType()) {
        ArrayType *arrTy = cast<ArrayType>(arrExpr->getType());
        setType(arrTy->getComponentType());
    }

    indexExprs = new Expr*[numIndices];
    std::copy(indices, indices + numIndices, indexExprs);
}

//===----------------------------------------------------------------------===//
// StringLiteral

void StringLiteral::init(const char *string, unsigned len)
{
    this->rep = new char[len];
    this->len = len;
    std::strncpy(this->rep, string, len);
}

StringLiteral::const_component_iterator
StringLiteral::findComponent(EnumerationType *type) const
{
    EnumerationType *root = type->getRootType();

    const_component_iterator I = begin_component_types();
    const_component_iterator E = end_component_types();
    for ( ; I != E; ++I) {
        const EnumerationDecl *decl = *I;
        if (root == decl->getType()->getRootType())
            return I;
    }
    return E;
}

StringLiteral::component_iterator
StringLiteral::findComponent(EnumerationType *type)
{
    EnumerationType *root = type->getRootType();

    component_iterator I = begin_component_types();
    component_iterator E = end_component_types();
    for ( ; I != E; ++I) {
        EnumerationDecl *decl = *I;
        if (root == decl->getType()->getRootType())
            return I;
    }
    return E;
}

bool StringLiteral::resolveComponentType(EnumerationType *type)
{
    component_iterator I = findComponent(type);

    if (I == end_component_types())
        return false;

    EnumerationDecl *decl = *I;
    interps.clear();
    interps.insert(decl);
    return true;
}

//===----------------------------------------------------------------------===//
// DereferenceExpr

DereferenceExpr::DereferenceExpr(Expr *prefix, Location loc, bool isImplicit)
    : Expr(AST_DereferenceExpr, loc),
      prefix(prefix)
{
    AccessType *prefixType = cast<AccessType>(prefix->getType());
    setType(prefixType->getTargetType());
}
