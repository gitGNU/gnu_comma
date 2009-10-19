//===-- ast/DeclRewriter.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DeclRewriter.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void DeclRewriter::mirrorRegion(DeclRegion *source, DeclRegion *target)
{
    typedef DeclRegion::DeclIter iterator;
    iterator I = source->beginDecls();
    iterator E = source->endDecls();

    for ( ; I != E; ++I) {
        Decl *sourceDecl = *I;
        IdentifierInfo *idInfo = sourceDecl->getIdInfo();
        Type *sourceType = 0;

        if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(sourceDecl))
            sourceType = SR->getType();
        else if (TypeDecl *TD = dyn_cast<TypeDecl>(sourceDecl))
            sourceType = TD->getType();
        else {
            assert(false && "Cannot mirror this kind of declaration yet!");
            return;
        }

        Type *targetType = rewriteType(sourceType);
        Decl *targetDecl = target->findDecl(idInfo, targetType);
        assert(targetDecl && "Could not resolve declaration!");
        addDeclRewrite(sourceDecl, targetDecl);
    }
}

FunctionDecl *DeclRewriter::rewriteFunctionDecl(FunctionDecl *fdecl)
{
    llvm::SmallVector<ParamValueDecl*, 8> params;
    unsigned arity = fdecl->getArity();

    for (unsigned i = 0; i < arity; ++i) {
        ParamValueDecl *origParam = fdecl->getParam(i);
        Type *newType = rewriteType(origParam->getType());
        ParamValueDecl *newParam =
            new ParamValueDecl(origParam->getIdInfo(), newType,
                               origParam->getExplicitParameterMode(), 0);
        params.push_back(newParam);
    }

    FunctionDecl *result =
        new FunctionDecl(getAstResource(),
                         fdecl->getIdInfo(), 0, params.data(), arity,
                         rewriteType(fdecl->getReturnType()), context);
    result->setOrigin(fdecl);
    addDeclRewrite(fdecl, result);
    return result;
}

ProcedureDecl *DeclRewriter::rewriteProcedureDecl(ProcedureDecl *pdecl)
{
    llvm::SmallVector<ParamValueDecl*, 8> params;
    unsigned arity = pdecl->getArity();

    for (unsigned i = 0; i < arity; ++i) {
        ParamValueDecl *origParam = pdecl->getParam(i);
        Type *newType = rewriteType(origParam->getType());
        ParamValueDecl *newParam =
            new ParamValueDecl(origParam->getIdInfo(), newType,
                               origParam->getExplicitParameterMode(), 0);
        params.push_back(newParam);
    }

    ProcedureDecl *result =
        new ProcedureDecl(getAstResource(),
                          pdecl->getIdInfo(), 0, params.data(), arity, context);
    result->setOrigin(pdecl);
    addDeclRewrite(pdecl, result);
    return result;
}

EnumerationDecl *
DeclRewriter::rewriteEnumerationDecl(EnumerationDecl *edecl)
{
    typedef std::pair<IdentifierInfo*, Location> Pair;

    IdentifierInfo *name = edecl->getIdInfo();
    llvm::SmallVector<Pair, 8> elems;

    // Enumeration declarations do not require any rewriting.  Just construct a
    // new declaration which is otherwise identical to the one given.
    //
    // Traverse the given decls region and extract each enumeration literal
    // therein.  Note that the DeclRegion of an enumeration contains
    // declarations for its primitive operations as well.
    typedef DeclRegion::DeclIter iterator;
    iterator I = edecl->beginDecls();
    iterator E = edecl->endDecls();
    for ( ; I != E; ++I) {
        EnumLiteral *lit = dyn_cast<EnumLiteral>(*I);
        if (!lit)
            continue;
        IdentifierInfo *litId = lit->getIdInfo();
        elems.push_back(Pair(litId, 0));
    }

    AstResource &resource = getAstResource();
    EnumerationDecl *result =
        resource.createEnumDecl(name, 0, &elems[0], elems.size(), context);
    result->generateImplicitDeclarations(resource);

    // Inject rewrite rules mapping the old enumeration to the new.
    addDeclRewrite(edecl, result);
    addTypeRewrite(edecl->getType(), result->getType());
    mirrorRegion(edecl, result);
    return result;
}

ArrayDecl *DeclRewriter::rewriteArrayDecl(ArrayDecl *adecl)
{
    IdentifierInfo *name = adecl->getIdInfo();
    unsigned rank = adecl->getRank();
    bool isConstrained = adecl->isConstrained();
    Type *component = rewriteType(adecl->getComponentType());
    SubType *indices[rank];

    for (unsigned i = 0; i < rank; ++i)
        indices[i] = cast<SubType>(rewriteType(adecl->getIndexType(i)));

    ArrayDecl *result;
    AstResource &resource = getAstResource();

    result = resource.createArrayDecl(name, 0, rank, indices,
                                      component, isConstrained, context);

    /// FIXME:  Array types will eventually have primitive operations defined on
    /// them.  Generate and mirror the results.
    addDeclRewrite(adecl, result);
    addTypeRewrite(adecl->getType(), result->getType());
    return result;
}

IntegerDecl *DeclRewriter::rewriteIntegerDecl(IntegerDecl *idecl)
{
    IdentifierInfo *name = idecl->getIdInfo();
    Expr *lower = rewriteExpr(idecl->getLowBoundExpr());
    Expr *upper = rewriteExpr(idecl->getHighBoundExpr());

    IntegerDecl *result;
    AstResource &resource = getAstResource();

    result = resource.createIntegerDecl(name, 0, lower, upper, context);
    result->generateImplicitDeclarations(resource);

    IntegerSubType *sourceTy = idecl->getType();
    IntegerSubType *targetTy = result->getType();

    // Provide mappings from the first subtypes and base types of the source and
    // target.
    addTypeRewrite(sourceTy, targetTy);
    addTypeRewrite(sourceTy->getTypeOf()->getBaseSubType(),
                   targetTy->getTypeOf()->getBaseSubType());

    addDeclRewrite(idecl, result);
    mirrorRegion(idecl, result);
    return result;
}

Decl *DeclRewriter::rewriteDecl(Decl *decl)
{
    Decl *result = 0;

    switch (decl->getKind()) {

    default:
        assert(false && "Do not yet know how to rewrite the given decl!");
        break;

    case Ast::AST_FunctionDecl:
        result = rewriteFunctionDecl(cast<FunctionDecl>(decl));
        break;

    case Ast::AST_ProcedureDecl:
        result = rewriteProcedureDecl(cast<ProcedureDecl>(decl));
        break;

    case Ast::AST_IntegerDecl:
        result = rewriteIntegerDecl(cast<IntegerDecl>(decl));
        break;

    case Ast::AST_EnumerationDecl:
        result = rewriteEnumerationDecl(cast<EnumerationDecl>(decl));
        break;

    case Ast::AST_ArrayDecl:
        result = rewriteArrayDecl(cast<ArrayDecl>(decl));
        break;
    };

    return result;
}

IntegerLiteral *DeclRewriter::rewriteIntegerLiteral(IntegerLiteral *lit)
{
    Type *targetTy = rewriteType(lit->getType());
    const llvm::APInt &value = lit->getValue();
    return new IntegerLiteral(value, targetTy, 0);
}

FunctionCallExpr *
DeclRewriter::rewriteFunctionCall(FunctionCallExpr *call)
{
    FunctionDecl *connective = rewriteFunctionDecl(call->getConnective());
    unsigned numArgs = call->getNumArgs();
    Expr *args[numArgs];

    // When rewriting function calls we pay no respect to any keyed arguments in
    // the source expression.  Just generate a positional call.
    FunctionCallExpr::arg_iterator I = call->begin_arguments();
    FunctionCallExpr::arg_iterator E = call->end_arguments();
    for (unsigned idx = 0; I != E; ++I, ++idx)
        args[idx] = rewriteExpr(*I);

    SubroutineRef *ref = new SubroutineRef(0, connective);
    return new FunctionCallExpr(ref, args, numArgs, 0, 0);
}

Expr *DeclRewriter::rewriteExpr(Expr *expr)
{
    Expr *result = 0;

    switch (expr->getKind()) {

    default:
        assert(false && "Cannot rewrite this kind of expr yet!");
        break;

    case Ast::AST_FunctionCallExpr:
        result = rewriteFunctionCall(cast<FunctionCallExpr>(expr));
        break;

    case Ast::AST_IntegerLiteral:
        result = rewriteIntegerLiteral(cast<IntegerLiteral>(expr));
        break;
    };

    return result;
}
