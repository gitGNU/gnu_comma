//===-- ast/AstRewriter.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstRewriter.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/SmallVector.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

Type *AstRewriter::getRewrite(Type *source) const
{
    RewriteMap::const_iterator iter = rewrites.find(source);
    if (iter == rewrites.end())
        return source;
    return iter->second;
}

void AstRewriter::installRewrites(DomainType *context)
{
    if (context->denotesPercent()) return;

    ModelDecl *model = context->getDeclaration();

    addRewrite(model->getPercent(), context);

    if (DomainInstanceDecl *instance = context->getInstanceDecl()) {
        if (FunctorDecl *functor = instance->getDefiningFunctor()) {
            unsigned arity = instance->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                DomainType *formal = functor->getFormalDomain(i);
                Type       *actual = instance->getActualParameter(i);
                rewrites[formal] = actual;
            }
        }
    }
}

void AstRewriter::installRewrites(SignatureType *context)
{
    VarietyDecl *variety = context->getVariety();

    if (variety) {
        unsigned arity = variety->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            DomainType *formal = variety->getFormalDomain(i);
            Type       *actual = context->getActualParameter(i);
            addRewrite(formal, actual);
        }
    }
}

SignatureType *AstRewriter::rewrite(SignatureType *sig) const
{
    if (sig->isParameterized()) {
        llvm::SmallVector<Type*, 4> args;
        SignatureType::arg_iterator iter;
        SignatureType::arg_iterator endIter = sig->endArguments();
        for (iter = sig->beginArguments(); iter != endIter; ++iter) {
            if (Type *dom = getRewrite(*iter))
                args.push_back(dom);
            else
                args.push_back(*iter);
        }
        // Obtain a memoized instance of this type.
        VarietyDecl *decl = sig->getVariety();
        return decl->getCorrespondingType(&args[0], args.size());
    }
    return sig;
}


DomainType *AstRewriter::rewrite(DomainType *dom) const
{
    if (DomainInstanceDecl *instance = dom->getInstanceDecl()) {
        if (FunctorDecl *functor = instance->getDefiningFunctor()) {
            typedef DomainInstanceDecl::arg_iterator iterator;
            llvm::SmallVector<Type*, 4> args;

            iterator iter;
            iterator endIter = instance->endArguments();
            for (iter = instance->beginArguments(); iter != endIter; ++iter) {
                // If the argument is a member of the rewrite set, then we must
                // create a new
                if (Type *target = getRewrite(*iter))
                    args.push_back(target);
                else
                    args.push_back(*iter);
            }
            // Obtain a memoized instance and return the associated type.
            instance = functor->getInstance(&args[0], args.size());
            return instance->getType();
        }
    }
    return dom;
}

SubroutineType *AstRewriter::rewrite(SubroutineType *srType) const
{
    if (ProcedureType *ptype = dyn_cast<ProcedureType>(srType))
        return rewrite(ptype);

    return rewrite(cast<FunctionType>(srType));
}

// Rewrites "count" parameter types of the given subroutine, placing the results
// of the rewrite in "params".
void AstRewriter::rewriteParameters(SubroutineType *srType,
                                    unsigned        count,
                                    Type          **params) const
{
    Type *source;
    Type *target;

    for (unsigned i = 0; i < count; ++i) {
        source = srType->getArgType(i);
        target = getRewrite(source);
        if (target)
            params[i] = target;
        else
            params[i] = source;
    }
}

FunctionType *AstRewriter::rewrite(FunctionType *ftype) const
{
    unsigned         arity = ftype->getArity();
    Type            *params[arity];
    IdentifierInfo **keywords;
    Type            *source;
    Type            *target;

    rewriteParameters(ftype, arity, params);
    keywords = ftype->getKeywordArray();
    source   = ftype->getReturnType();
    target   = getRewrite(source);
    if (target)
        return new FunctionType(keywords, params, arity, target);
    else
        return new FunctionType(keywords, params, arity, source);
}

ProcedureType *AstRewriter::rewrite(ProcedureType *ptype) const
{
    unsigned         arity = ptype->getArity();
    Type            *params[arity];
    IdentifierInfo **keywords;
    ProcedureType   *result;

    rewriteParameters(ptype, arity, params);
    keywords = ptype->getKeywordArray();
    result   = new ProcedureType(keywords, &params[0], arity);

    for (unsigned i = 0; i < arity; ++i) {
        ParameterMode mode = ptype->getExplicitParameterMode(i);
        result->setParameterMode(mode, i);
    }

    return result;
}

