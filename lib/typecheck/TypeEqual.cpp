//===-- typecheck/TypeEqual.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "TypeEqual.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::isa;

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      SignatureType     *typeX,
                                      SignatureType     *typeY)
{
    if (typeX->getDeclaration() == typeY->getDeclaration()) {
        if (typeX->isParameterized()) {
            unsigned arity = typeX->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                DomainType *argX = typeX->getActualParameter(i);
                DomainType *argY = typeY->getActualParameter(i);
                if (!compareTypesUsingRewrites(rewrites, argX, argY))
                    return false;
            }
        }
        return true;
    }
    return false;
}

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      DomainType        *typeX,
                                      DomainType        *typeY)
{
    typeX = rewrites.getRewrite(typeX);
    typeY = rewrites.getRewrite(typeY);

    if (typeX == typeY)
        return true;

    // Otherwise, typeX and typeY must be instances of the same functor for the
    // comparison to succeed since all non-parameterized types are represented
    // by a unique node.
    if (typeX->getDeclaration() == typeY->getDeclaration()) {

        if (!(typeX->isParameterized() && typeY->isParameterized()))
            return false;

        // We know the arity of both types are the same since they are supported
        // by identical declarations.
        unsigned arity = typeX->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            DomainType *argX = typeX->getActualParameter(i);
            DomainType *argY = typeY->getActualParameter(i);
            if (!compareTypesUsingRewrites(rewrites, argX, argY))
                return false;
        }
        return true;
    }
    return false;
}

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      FunctionType      *typeX,
                                      FunctionType      *typeY)
{
    unsigned arity = typeX->getArity();

    if (arity != typeY->getArity())
        return false;

    if (!compareTypesUsingRewrites(rewrites,
                                   typeX->getReturnType(),
                                   typeY->getReturnType()))
        return false;

    for (unsigned i = 0; i < arity; ++i) {
        DomainType *argX = typeX->getArgType(i);
        DomainType *argY = typeY->getArgType(i);
        if (!compareTypesUsingRewrites(rewrites, argX, argY))
            return false;
    }

    return true;
}
