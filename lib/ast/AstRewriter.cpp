//===-- ast/AstRewriter.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/AstRewriter.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"

#include "llvm/Support/Casting.h"
#include "llvm/ADT/SmallVector.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

Type *AstRewriter::findRewrite(Type *source) const
{
    RewriteMap::const_iterator iter = rewrites.find(source);
    if (iter == rewrites.end())
        return 0;
    return iter->second;
}

Type *AstRewriter::getRewrite(Type *source) const
{
    if (Type *res = findRewrite(source))
        return res;
    return source;
}

void AstRewriter::installRewrites(DomainType *context)
{
    // If this type is parameterized, install rewites mapping the formal
    // arguments of the underlying functor to the actual arguments of the type.
    if (DomainInstanceDecl *instance = context->getInstanceDecl()) {
        if (FunctorDecl *functor = instance->getDefiningFunctor()) {
            unsigned arity = instance->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                DomainType *formal = functor->getFormalType(i);
                Type *actual = instance->getActualParamType(i);
                rewrites[formal] = actual;
            }
        }
    }
}

void AstRewriter::installRewrites(SigInstanceDecl *context)
{
    VarietyDecl *variety = context->getVariety();

    if (variety) {
        unsigned arity = variety->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            DomainType *formal = variety->getFormalType(i);
            Type *actual = context->getActualParamType(i);
            addTypeRewrite(formal, actual);
        }
    }
}

Type *AstRewriter::rewriteType(Type *type) const
{
    if (Type *result = findRewrite(type))
        return result;

    switch(type->getKind()) {

    default: return type;

    case Ast::AST_DomainType:
        return rewriteType(cast<DomainType>(type));
    case Ast::AST_FunctionType:
        return rewriteType(cast<FunctionType>(type));
    case Ast::AST_ProcedureType:
        return rewriteType(cast<ProcedureType>(type));
    }
}

SigInstanceDecl *AstRewriter::rewriteSigInstance(SigInstanceDecl *sig) const
{
    if (sig->isParameterized()) {
        llvm::SmallVector<DomainTypeDecl*, 4> args;
        SigInstanceDecl::arg_iterator iter;
        SigInstanceDecl::arg_iterator endIter = sig->endArguments();
        for (iter = sig->beginArguments(); iter != endIter; ++iter) {
            // FIXME: Currently it is true that all arguments are domains, but
            // in the future we will need to be more general than this.
            DomainType *argTy = rewriteType((*iter)->getType());
            args.push_back(argTy->getDomainTypeDecl());
        }
        // Obtain a memoized instance.
        VarietyDecl *decl = sig->getVariety();
        return decl->getInstance(&args[0], args.size());
    }
    return sig;
}

DomainType *AstRewriter::rewriteType(DomainType *dom) const
{
    if (DomainType *result = dyn_cast_or_null<DomainType>(findRewrite(dom)))
        return result;

    if (DomainInstanceDecl *instance = dom->getInstanceDecl()) {
        if (FunctorDecl *functor = instance->getDefiningFunctor()) {
            typedef DomainInstanceDecl::arg_iterator iterator;
            llvm::SmallVector<DomainTypeDecl*, 4> args;
            iterator iter;
            iterator endIter = instance->endArguments();
            for (iter = instance->beginArguments(); iter != endIter; ++iter) {
                // FIXME: Currently it is true that all arguments are domains,
                // but in the future we will need to be more general than this.
                DomainType *argTy = rewriteType((*iter)->getType());
                args.push_back(argTy->getDomainTypeDecl());
            }
            // Obtain a memoized instance and return the associated type.
            instance = functor->getInstance(&args[0], args.size());
            return instance->getType();
        }
    }
    return dom;
}

SubroutineType *AstRewriter::rewriteType(SubroutineType *srType) const
{
    if (ProcedureType *ptype = dyn_cast<ProcedureType>(srType))
        return rewriteType(ptype);

    return rewriteType(cast<FunctionType>(srType));
}

void AstRewriter::rewriteParameters(SubroutineType *srType,
                                    unsigned count, Type **params) const
{
    for (unsigned i = 0; i < count; ++i)
        params[i] = getRewrite(srType->getArgType(i));
}

FunctionType *AstRewriter::rewriteType(FunctionType *ftype) const
{
    unsigned arity = ftype->getArity();
    Type *returnType = getRewrite(ftype->getReturnType());
    Type *paramTypes[arity];

    rewriteParameters(ftype, arity, paramTypes);
    return resource.getFunctionType(paramTypes, arity, returnType);
}

ProcedureType *AstRewriter::rewriteType(ProcedureType *ptype) const
{
    unsigned arity = ptype->getArity();
    Type *paramTypes[arity];

    rewriteParameters(ptype, arity, paramTypes);
    return resource.getProcedureType(paramTypes, arity);
}

