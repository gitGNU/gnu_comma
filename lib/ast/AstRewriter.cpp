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

using namespace comma;

SignatureType *AstRewriter::rewrite(SignatureType *sig)
{
    std::vector<ModelType *> args;

    if (sig->isParameterized()) {
        SignatureType::arg_iterator iter;
        SignatureType::arg_iterator endIter = sig->endArguments();
        for (iter = sig->beginArguments(); iter != endIter; ++iter) {
            // If the argument is a member of the rewrite set, then we must
            // create a new
            if (ModelType *dom = static_cast<ModelType *>(rewrites[*iter]))
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


DomainType *AstRewriter::rewrite(DomainType *dom)
{
    std::vector<ModelType *> args;

    if (dom->isParameterized()) {
        DomainType::arg_iterator iter;
        DomainType::arg_iterator endIter = dom->endArguments();
        for (iter = dom->beginArguments(); iter != endIter; ++iter) {
            // If the argument is a member of the rewrite set, then we must
            // create a new
            if (ModelType *dom = static_cast<ModelType*>(rewrites[*iter]))
                args.push_back(dom);
            else
                args.push_back(*iter);
        }
        // Obtain a memoized instance of this type.
        FunctorDecl *decl = dom->getFunctor();
        return decl->getCorrespondingType(&args[0], args.size());

    }
    return dom;
}
