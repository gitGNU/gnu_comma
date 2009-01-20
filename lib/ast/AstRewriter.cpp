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
    if (isa<PercentType>(context)) return;

    ModelDecl *model = context->getDeclaration();
    addRewrite(model->getPercent(), context);

    if (ConcreteDomainType *domain = dyn_cast<ConcreteDomainType>(context)) {
        if (domain->isParameterized()) {
            FunctorDecl *functor = domain->getFunctor();
            unsigned arity = functor->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                AbstractDomainType *formal = functor->getFormalDomain(i);
                DomainType *actual = domain->getActualParameter(i);
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
            AbstractDomainType *formal = variety->getFormalDomain(i);
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
    ConcreteDomainType *source = llvm::dyn_cast<ConcreteDomainType>(dom);

    if (source && source->isParameterized()) {
        llvm::SmallVector<DomainType*, 4> args;

        ConcreteDomainType::arg_iterator iter;
        ConcreteDomainType::arg_iterator endIter = source->endArguments();
        for (iter = source->beginArguments(); iter != endIter; ++iter) {
            // If the argument is a member of the rewrite set, then we must
            // create a new
            if (DomainType *target = getRewrite(*iter))
                args.push_back(target);
            else
                args.push_back(*iter);
        }
        // Obtain a memoized instance of this type.
        FunctorDecl *decl = source->getFunctor();
        return decl->getCorrespondingType(&args[0], args.size());

    }
    return dom;
}

FunctionType *AstRewriter::rewrite(FunctionType *ftype) const
{
    llvm::SmallVector<IdentifierInfo*, 4> formals;
    llvm::SmallVector<DomainType*, 4> args;
    DomainType *source;
    DomainType *target;
    unsigned    arity;

    arity = ftype->getArity();
    for (unsigned i = 0; i < arity; ++i) {
        source = ftype->getArgType(i);
        target = getRewrite(source);
        if (target)
            args.push_back(target);
        else
            args.push_back(source);
        formals.push_back(ftype->getSelector(i));
    }
    source = ftype->getReturnType();
    target = getRewrite(source);
    if (target)
        return new FunctionType(&formals[0], &args[0], args.size(), target);
    else
        return new FunctionType(&formals[0], &args[0], args.size(), source);
}

