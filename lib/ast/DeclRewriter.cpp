//===-- ast/DeclRewriter.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DeclRewriter.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/Expr.h"

using namespace comma;
using llvm::cast_or_null;
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
                               origParam->getExplicitParameterMode(),
                               Location());
        params.push_back(newParam);
    }

    FunctionDecl *result =
        new FunctionDecl(getAstResource(),
                         fdecl->getIdInfo(), fdecl->getLocation(),
                         params.data(), arity,
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
                               origParam->getExplicitParameterMode(),
                               origParam->getLocation());
        params.push_back(newParam);
    }

    ProcedureDecl *result =
        new ProcedureDecl(getAstResource(),
                          pdecl->getIdInfo(), pdecl->getLocation(),
                          params.data(), arity, context);
    result->setOrigin(pdecl);
    addDeclRewrite(pdecl, result);
    return result;
}

EnumerationDecl *
DeclRewriter::rewriteEnumerationDecl(EnumerationDecl *edecl)
{
    typedef std::pair<IdentifierInfo*, Location> Pair;

    AstResource &resource = getAstResource();
    IdentifierInfo *name = edecl->getIdInfo();
    Location loc = edecl->getLocation();
    EnumerationDecl *result;

    if (edecl->isSubtypeDeclaration()) {
        EnumerationType *etype = edecl->getType();
        EnumerationType *base = etype->getAncestorType();

        if (Range *range = etype->getConstraint()) {
            Expr *lower = rewriteExpr(range->getLowerBound());
            Expr *upper = rewriteExpr(range->getUpperBound());
            result = resource.createEnumSubtypeDecl
                (name, loc, base, lower, upper, context);
        }
        else
            result = resource.createEnumSubtypeDecl(name, loc, base, context);
    }
    else {
        // Enumeration type declarations do not require any rewriting.  Just
        // construct a new declaration which is otherwise identical to the one
        // given.
        //
        // Traverse the given decls region and extract each enumeration literal
        // therein.  Note that the DeclRegion of an enumeration contains
        // declarations for its primitive operations as well.
        typedef DeclRegion::DeclIter iterator;
        iterator I = edecl->beginDecls();
        iterator E = edecl->endDecls();
        llvm::SmallVector<Pair, 8> elems;
        for ( ; I != E; ++I) {
            EnumLiteral *lit = dyn_cast<EnumLiteral>(*I);
            if (!lit)
                continue;
            IdentifierInfo *litId = lit->getIdInfo();
            elems.push_back(Pair(litId, lit->getLocation()));
        }

        result = resource.createEnumDecl
            (name, loc, &elems[0], elems.size(), context);
        result->generateImplicitDeclarations(resource);
    }

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
    llvm::SmallVector<DSTDefinition*, 16> indices(rank);

    for (unsigned i = 0; i < rank; ++i) {
        // Just rewrite the index types into DSTDef's with the Type_DST tag.
        // There is no loss of type information (but the original syntax of the
        // declaration is gone, which is OK for a rewritten node).
        //
        // FIXME: It should be possible to construct an ArrayDecl over a set of
        // DiscreteType's instead of always requiring DSTDef's.
        DiscreteType *indexTy;
        Location loc = adecl->getDSTDefinition(i)->getLocation();
        indexTy = cast<DiscreteType>(rewriteType(adecl->getIndexType(i)));
        indices[i] = new DSTDefinition(loc, indexTy, DSTDefinition::Type_DST);
    }

    ArrayDecl *result;
    AstResource &resource = getAstResource();
    result = resource.createArrayDecl(name, adecl->getLocation(),
                                      rank, &indices[0],
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
    Location loc = idecl->getLocation();

    IntegerDecl *result;
    AstResource &resource = getAstResource();

    if (idecl->isSubtypeDeclaration()) {
        IntegerType *type = idecl->getType();
        IntegerType *base = type->getAncestorType();

        if (type->isConstrained()) {
            Expr *lower = rewriteExpr(idecl->getLowBoundExpr());
            Expr *upper = rewriteExpr(idecl->getHighBoundExpr());
            result = resource.createIntegerSubtypeDecl
                (name, loc, base, lower, upper, context);
        }
        else
            result = resource.createIntegerSubtypeDecl
                (name, loc, base, context);
    }
    else if (idecl->isModularDeclaration()) {
        Expr *modulus = rewriteExpr(idecl->getModulusExpr());
        result = resource.createIntegerDecl(name, loc, modulus, context);
        result->generateImplicitDeclarations(resource);
    }
    else {
        Expr *lower = rewriteExpr(idecl->getLowBoundExpr());
        Expr *upper = rewriteExpr(idecl->getHighBoundExpr());
        result = resource.createIntegerDecl(name, loc, lower, upper, context);
        result->generateImplicitDeclarations(resource);
    }

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
    RecordDecl *result;
    if ((result = cast_or_null<RecordDecl>(findRewrite(decl))))
        return result;

    IdentifierInfo *name = decl->getIdInfo();
    AstResource &resource = getAstResource();
    result = resource.createRecordDecl(name, decl->getLocation(), context);

    typedef DeclRegion::DeclIter decl_iterator;
    decl_iterator I = decl->beginDecls();
    decl_iterator E = decl->endDecls();
    for ( ; I != E; ++I) {
        if (ComponentDecl *orig = dyn_cast<ComponentDecl>(*I)) {
            IdentifierInfo *componentID = orig->getIdInfo();
            Location componentLoc = orig->getLocation();
            Type *componentTy = rewriteType(orig->getType());
            result->addComponent(componentID, componentLoc, componentTy);
        }
    }

    // Provide mappings from the original first subtype to the new subtype.
    addTypeRewrite(decl->getType(), result->getType());
    addDeclRewrite(decl, result);
    return result;
}

IncompleteTypeDecl *
DeclRewriter::rewriteIncompleteTypeDecl(IncompleteTypeDecl *ITD)
{
    IncompleteTypeDecl *result;
    if ((result = cast_or_null<IncompleteTypeDecl>(findRewrite(ITD))))
        return result;

    IdentifierInfo *name = ITD->getIdInfo();
    Location loc = ITD->getLocation();
    result = getAstResource().createIncompleteTypeDecl(name, loc, context);

    // Provide a mapping from the original declaration to the new one.  We do
    // this before rewriting the completion (if any) to avoid circularites.
    addTypeRewrite(ITD->getType(), result->getType());
    addDeclRewrite(ITD, result);

    // The new incomplete type declaration does not have a completion.  If the
    // given ITD has a completion rewrite it as well.
    if (ITD->hasCompletion()) {
        TypeDecl *completion = rewriteTypeDecl(ITD->getCompletion());
        result->setCompletion(completion);
    }

    return result;
}

CarrierDecl *DeclRewriter::rewriteCarrierDecl(CarrierDecl *carrier)
{
    CarrierDecl *result;
    if ((result = cast_or_null<CarrierDecl>(findRewrite(carrier))))
        return result;

    IdentifierInfo *name = carrier->getIdInfo();
    Location loc = carrier->getLocation();
    PrimaryType *rep = cast<PrimaryType>(rewriteType(carrier->getType()));
    result = new CarrierDecl(getAstResource(), name, rep, loc);

    addTypeRewrite(carrier->getType(), result->getType());
    addDeclRewrite(carrier, result);
    return result;
}

AccessDecl *DeclRewriter::rewriteAccessDecl(AccessDecl *access)
{
    AccessDecl *result;
    if ((result = cast_or_null<AccessDecl>(findRewrite(access))))
        return result;

    AstResource &resource = getAstResource();
    IdentifierInfo *name = access->getIdInfo();
    Location loc = access->getLocation();
    Type *targetType = rewriteType(access->getType()->getTargetType());

    // An access type can ultimately reference itself via the target type.  If
    // we have an entry to ourselves we know a circularity is present and we are
    // done.
    //
    // FIXME: It might be better to construct an access decl without a target
    // type and insert it into the rewrite map immediately so that any recursive
    // references can simply be resolved.  OTOH, the current approach depends on
    // the rewriter "bottoming out" thru an incomplete type when circularities
    // are present.  In some sense, the current implementation "tests" that the
    // AST is sane at the expense of speed.
    if ((result = cast_or_null<AccessDecl>(findRewrite(access))))
        return result;

    result = resource.createAccessDecl(name, loc, targetType, context);
    result->generateImplicitDeclarations(resource);
    result->setOrigin(access);
    addTypeRewrite(access->getType(), result->getType());
    addDeclRewrite(access, result);
    mirrorRegion(access, result);
    return result;
}

TypeDecl *DeclRewriter::rewriteTypeDecl(TypeDecl *decl)
{
    TypeDecl *result = 0;

    switch (decl->getKind()) {

    default:
        assert(false && "Do not yet know how to rewrite the given decl!");
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

    case Ast::AST_IncompleteTypeDecl:
        result = rewriteIncompleteTypeDecl(cast<IncompleteTypeDecl>(decl));
        break;

    case Ast::AST_CarrierDecl:
        result = rewriteCarrierDecl(cast<CarrierDecl>(decl));
        break;

    case Ast::AST_AccessDecl:
        result = rewriteAccessDecl(cast<AccessDecl>(decl));
        break;

    }

    return result;
}

Decl *DeclRewriter::rewriteDecl(Decl *decl)
{
    Decl *result = 0;
    if ((result = findRewrite(decl)))
        return result;

    switch (decl->getKind()) {

    default:
        result = rewriteTypeDecl(cast<TypeDecl>(decl));
        break;

    case Ast::AST_FunctionDecl:
        result = rewriteFunctionDecl(cast<FunctionDecl>(decl));
        break;

    case Ast::AST_ProcedureDecl:
        result = rewriteProcedureDecl(cast<ProcedureDecl>(decl));
        break;
    };

    return result;
}

Type *DeclRewriter::rewriteType(Type *type)
{
    Type *result = 0;
    if ((result = AstRewriter::findRewrite(type)))
        return result;

    switch (type->getKind()) {

    default:
        result = AstRewriter::rewriteType(type);
        break;

    case Ast::AST_AccessType:
        result = rewriteAccessType(cast<AccessType>(type));
        break;

    case Ast::AST_RecordType:
        result = rewriteRecordType(cast<RecordType>(type));
        break;

    case Ast::AST_IncompleteType:
        result = rewriteIncompleteType(cast<IncompleteType>(type));
        break;

    case Ast::AST_ArrayType:
        result = rewriteArrayType(cast<ArrayType>(type));
        break;
    }
    return result;
}

AccessType *DeclRewriter::rewriteAccessType(AccessType *type)
{
    AccessDecl *declaration = type->getDefiningDecl();
    if (declaration->getDeclRegion() != origin)
        return type;
    return rewriteAccessDecl(declaration)->getType();
}

RecordType *DeclRewriter::rewriteRecordType(RecordType *type)
{
    RecordDecl *declaration = type->getDefiningDecl();
    if (declaration->getDeclRegion() != origin)
        return type;
    return rewriteRecordDecl(declaration)->getType();
}

IncompleteType *DeclRewriter::rewriteIncompleteType(IncompleteType *type)
{
    IncompleteTypeDecl *declaration = type->getDefiningDecl();
    if (declaration->getDeclRegion() != origin)
        return type;
    return rewriteIncompleteTypeDecl(declaration)->getType();
}

ArrayType *DeclRewriter::rewriteArrayType(ArrayType *type)
{
    ArrayDecl *declaration = type->getDefiningDecl();
    if (declaration->getDeclRegion() != origin)
        return type;
    return rewriteArrayDecl(declaration)->getType();
}

IntegerLiteral *DeclRewriter::rewriteIntegerLiteral(IntegerLiteral *lit)
{
    IntegerType *targetTy = cast<IntegerType>(rewriteType(lit->getType()));
    const llvm::APInt &value = lit->getValue();
    return new IntegerLiteral(value, targetTy, lit->getLocation());
}

FunctionCallExpr *
DeclRewriter::rewriteFunctionCall(FunctionCallExpr *call)
{
    FunctionDecl *connective = call->getConnective();
    Location loc = call->getLocation();
    unsigned numArgs = call->getNumArgs();
    llvm::SmallVector<Expr*, 16> args(numArgs);

    if (Decl *rewrite = findRewrite(connective))
        connective = cast<FunctionDecl>(rewrite);

    // When rewriting function calls we pay no respect to any keyed arguments in
    // the source expression.  Just generate a positional call.
    FunctionCallExpr::arg_iterator I = call->begin_arguments();
    FunctionCallExpr::arg_iterator E = call->end_arguments();
    for (unsigned idx = 0; I != E; ++I, ++idx)
        args[idx] = rewriteExpr(*I);

    return new FunctionCallExpr(connective, loc, args.data(), numArgs, 0, 0);
}

AttribExpr *DeclRewriter::rewriteAttrib(AttribExpr *attrib)
{
    AttribExpr *result = 0;
    Location loc = attrib->getLocation();

    if (ScalarBoundAE *bound = dyn_cast<ScalarBoundAE>(attrib)) {
        IntegerType *prefix;
        prefix = cast<IntegerType>(rewriteType(bound->getPrefix()));

        if (bound->isFirst())
            result = new FirstAE(prefix, loc);
        else
            result = new LastAE(prefix, loc);
    }
    if (LengthAE *length = dyn_cast<LengthAE>(attrib)) {
        // FIXME: Support array subtype prefix.
        Expr *prefix = length->getPrefixExpr();
        assert(prefix && "Cannot rewrite attribute!");

        prefix = rewriteExpr(prefix);
        if (length->hasImplicitDimension())
            result = new LengthAE(prefix, loc);
        else {
            Expr *dimension = rewriteExpr(length->getDimensionExpr());
            result = new LengthAE(prefix, loc, dimension);
        }
    }
    else {
        ArrayBoundAE *bound = cast<ArrayBoundAE>(attrib);
        Expr *prefix = rewriteExpr(bound->getPrefix());

        if (bound->hasImplicitDimension()) {
            if (bound->isFirst())
                result = new FirstArrayAE(prefix, loc);
            else
                result = new LastArrayAE(prefix, loc);
        }
        else {
            Expr *dim = rewriteExpr(bound->getDimensionExpr());
            if (bound->isFirst())
                result = new FirstArrayAE(prefix, dim, loc);
            else
                result = new LastArrayAE(prefix, dim, loc);
        }
    }
    return result;
}

ConversionExpr *DeclRewriter::rewriteConversion(ConversionExpr *conv)
{
    Expr *operand = rewriteExpr(conv->getOperand());
    Type *targetTy = rewriteType(conv->getType());
    return new ConversionExpr(operand, targetTy, conv->getLocation());
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
