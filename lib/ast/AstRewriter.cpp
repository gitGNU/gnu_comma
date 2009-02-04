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

DomainType *AstRewriter::getRewrite(DomainType *source) const
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

    if (context->isParameterized()) {
        FunctorDecl *functor = context->getFunctorDecl();
        unsigned arity = functor->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            DomainType *formal = functor->getFormalDomain(i);
            DomainType *actual = context->getActualParameter(i);
            rewrites[formal] = actual;
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
            DomainType *actual = context->getActualParameter(i);
            addRewrite(formal, actual);
        }
    }
}

SignatureType *AstRewriter::rewrite(SignatureType *sig) const
{
    if (sig->isParameterized()) {
        llvm::SmallVector<DomainType*, 4> args;
        SignatureType::arg_iterator iter;
        SignatureType::arg_iterator endIter = sig->endArguments();
        for (iter = sig->beginArguments(); iter != endIter; ++iter) {
            if (DomainType *dom = getRewrite(*iter))
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
    if (dom->isParameterized()) {
        llvm::SmallVector<DomainType*, 4> args;

        DomainType::arg_iterator iter;
        DomainType::arg_iterator endIter = dom->endArguments();
        for (iter = dom->beginArguments(); iter != endIter; ++iter) {
            // If the argument is a member of the rewrite set, then we must
            // create a new
            if (DomainType *target = getRewrite(*iter))
                args.push_back(target);
            else
                args.push_back(*iter);
        }
        // Obtain a memoized instance of this type.
        FunctorDecl *decl = dom->getFunctorDecl();
        return decl->getCorrespondingType(&args[0], args.size());

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
                                    DomainType    **params) const
{
    DomainType *source;
    DomainType *target;

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
    DomainType      *params[arity];
    IdentifierInfo **keywords;
    DomainType      *source;
    DomainType      *target;

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
    DomainType      *params[arity];
    IdentifierInfo **keywords;

    rewriteParameters(ptype, arity, params);
    keywords = ptype->getKeywordArray();
    return new ProcedureType(keywords, &params[0], arity);
}

