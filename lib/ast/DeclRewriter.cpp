//===-- ast/DeclRewriter.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DeclRewriter.h"
#include "comma/ast/DSTDefinition.h"
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
    result->setOrigin(edecl);

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
    DSTDefinition *indices[rank];

    for (unsigned i = 0; i < rank; ++i) {
        // Just rewrite the index types into DSTDef's with the Type_DST tag.
        // There is no loss of type information (but the original syntax of the
        // declaration is gone, which is OK for a rewritten node).
        //
        // FIXME: It should be possible to construct an ArrayDecl over a set of
        // DiscreteType's instead of always requiring DSTDef's.
        DiscreteType *indexTy;
        indexTy = cast<DiscreteType>(rewriteType(adecl->getIndexType(i)));
        indices[i] = new DSTDefinition(0, indexTy, DSTDefinition::Type_DST);
    }

    ArrayDecl *result;
    AstResource &resource = getAstResource();
    result = resource.createArrayDecl(name, 0, rank, &indices[0],
                                      component, isConstrained, context);
    result->setOrigin(adecl);

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
    result->setOrigin(idecl);

    IntegerType *sourceTy = idecl->getType();
    IntegerType *targetTy = result->getType();

    // Provide mappings from the first subtypes and base types of the source and
    // target.
    addTypeRewrite(sourceTy, targetTy);
    addTypeRewrite(sourceTy->getRootType()->getBaseSubtype(),
                   targetTy->getRootType()->getBaseSubtype());

    addDeclRewrite(idecl, result);
    mirrorRegion(idecl, result);
    return result;
}

RecordDecl *DeclRewriter::rewriteRecordDecl(RecordDecl *decl)
{
    IdentifierInfo *name = decl->getIdInfo();
    AstResource &resource = getAstResource();
    RecordDecl *result = resource.createRecordDecl(name, 0, context);

    typedef DeclRegion::DeclIter decl_iterator;
    decl_iterator I = decl->beginDecls();
    decl_iterator E = decl->endDecls();
    for ( ; I != E; ++I) {
        if (ComponentDecl *orig = dyn_cast<ComponentDecl>(*I)) {
            Type *componentTy = rewriteType(orig->getType());
            result->addComponent(orig->getIdInfo(), 0, componentTy);
        }
    }

    // Provide mappings from the original first subtype to the new subtype.
    addTypeRewrite(decl->getType(), result->getType());
    addDeclRewrite(decl, result);
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

    case Ast::AST_RecordDecl:
        result = rewriteRecordDecl(cast<RecordDecl>(decl));
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
    FunctionDecl *connective = call->getConnective();
    unsigned numArgs = call->getNumArgs();
    Expr *args[numArgs];

    if (Decl *rewrite = findRewrite(connective))
        connective = cast<FunctionDecl>(rewrite);

    // When rewriting function calls we pay no respect to any keyed arguments in
    // the source expression.  Just generate a positional call.
    FunctionCallExpr::arg_iterator I = call->begin_arguments();
    FunctionCallExpr::arg_iterator E = call->end_arguments();
    for (unsigned idx = 0; I != E; ++I, ++idx)
        args[idx] = rewriteExpr(*I);

    SubroutineRef *ref = new SubroutineRef(0, connective);
    return new FunctionCallExpr(ref, args, numArgs, 0, 0);
}

AttribExpr *DeclRewriter::rewriteAttrib(AttribExpr *attrib)
{
    AttribExpr *result = 0;

    if (ScalarBoundAE *bound = dyn_cast<ScalarBoundAE>(attrib)) {
        IntegerType *prefix;
        prefix = cast<IntegerType>(rewriteType(bound->getPrefix()));

        if (bound->isFirst())
            result = new FirstAE(prefix, 0);
        else
            result = new LastAE(prefix, 0);
    }
    else {
        ArrayBoundAE *bound = cast<ArrayBoundAE>(attrib);
        Expr *prefix = rewriteExpr(bound->getPrefix());

        if (bound->hasImplicitDimension()) {
            if (bound->isFirst())
                result = new FirstArrayAE(prefix, 0);
            else
                result = new LastArrayAE(prefix, 0);
        }
        else {
            Expr *dim = rewriteExpr(bound->getDimensionExpr());
            if (bound->isFirst())
                result = new FirstArrayAE(prefix, dim, 0);
            else
                result = new LastArrayAE(prefix, dim, 0);
        }
    }
    return result;
}

ConversionExpr *DeclRewriter::rewriteConversion(ConversionExpr *conv)
{
    Expr *operand = rewriteExpr(conv->getOperand());
    Type *targetTy = rewriteType(conv->getType());
    return new ConversionExpr(operand, targetTy, 0);
}

Expr *DeclRewriter::rewriteExpr(Expr *expr)
{
    Expr *result = 0;

    switch (expr->getKind()) {

    default:
        if (AttribExpr *attrib = dyn_cast<AttribExpr>(expr))
            result = rewriteAttrib(attrib);
        else
            assert(false && "Cannot rewrite this kind of expr yet!");
        break;

    case Ast::AST_FunctionCallExpr:
        result = rewriteFunctionCall(cast<FunctionCallExpr>(expr));
        break;

    case Ast::AST_IntegerLiteral:
        result = rewriteIntegerLiteral(cast<IntegerLiteral>(expr));
        break;

    case Ast::AST_ConversionExpr:
        result = rewriteConversion(cast<ConversionExpr>(expr));
        break;
    };

    return result;
}
