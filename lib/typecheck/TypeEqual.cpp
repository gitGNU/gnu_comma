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
using llvm::cast;
using llvm::isa;

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      Type              *typeX,
                                      Type              *typeY)
{
    Ast::AstKind kind = typeX->getKind();

    if (kind != typeY->getKind()) return false;

    switch (kind) {

    default:
        assert(false && "Cannot handle node kind!");
        break;

    case Ast::AST_SignatureType:
        if (SignatureType *sigY = dyn_cast<SignatureType>(typeY)) {
            SignatureType *sigX = cast<SignatureType>(typeX);
            return compareTypesUsingRewrites(rewrites, sigX, sigY);
        }
        break;

    case Ast::AST_DomainType:
        if (DomainType *domY = dyn_cast<DomainType>(typeY)) {
            DomainType *domX = cast<DomainType>(typeX);
            return compareTypesUsingRewrites(rewrites, domX, domY);
        }
        break;

    case Ast::AST_FunctionType:
        if (FunctionType *funY = dyn_cast<FunctionType>(typeY)) {
            FunctionType *funX = cast<FunctionType>(typeX);
            return compareTypesUsingRewrites(rewrites, funX, funY);
        }
        break;

    case Ast::AST_ProcedureType:
        if (ProcedureType *procY = dyn_cast<ProcedureType>(typeY)) {
            ProcedureType *procX = cast<ProcedureType>(typeX);
            return compareTypesUsingRewrites(rewrites, procX, procY);
        }
        break;
    }

    return false;
}

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      SignatureType     *typeX,
                                      SignatureType     *typeY)
{
    if (typeX->getDeclaration() == typeY->getDeclaration()) {
        if (typeX->isParameterized()) {
            unsigned arity = typeX->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                Type *argX = typeX->getActualParameter(i);
                Type *argY = typeY->getActualParameter(i);
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
    Type *rewriteX = rewrites.getRewrite(typeX);
    Type *rewriteY = rewrites.getRewrite(typeY);

    if (rewriteX == rewriteY)
        return true;

    // Otherwise, typeX and typeY must be instances of the same functor for the
    // comparison to succeed since all non-parameterized types are represented
    // by a unique node.
    DomainType *domX = cast<DomainType>(rewriteX);
    DomainType *domY = cast<DomainType>(rewriteY);
    DomainInstanceDecl *instanceX = domX->getInstanceDecl();
    DomainInstanceDecl *instanceY = domY->getInstanceDecl();
    if (instanceX && instanceY) {

        if (instanceX->getDefiningDecl() != instanceY->getDefiningDecl())
            return false;

        // We know the arity of both types are the same since they are supported
        // by identical declarations.
        unsigned arity = instanceX->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            Type *argX = instanceX->getActualParameter(i);
            Type *argY = instanceY->getActualParameter(i);
            if (!compareTypesUsingRewrites(rewrites, argX, argY))
                return false;
        }
        return true;
    }
    return false;
}

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      SubroutineType    *typeX,
                                      SubroutineType    *typeY)
{
    if (FunctionType *ftypeX = dyn_cast<FunctionType>(typeX)) {
        FunctionType *ftypeY = dyn_cast<FunctionType>(typeY);
        if (ftypeY)
            return compareTypesUsingRewrites(rewrites, ftypeX, ftypeY);
        return false;
    }

    ProcedureType *ptypeX = cast<ProcedureType>(typeX);
    ProcedureType *ptypeY = dyn_cast<ProcedureType>(typeY);
    if (ptypeY)
        return compareTypesUsingRewrites(rewrites, ptypeX, ptypeY);
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
        Type *argX = typeX->getArgType(i);
        Type *argY = typeY->getArgType(i);
        if (!compareTypesUsingRewrites(rewrites, argX, argY))
            return false;
    }

    return true;
}

bool comma::compareTypesUsingRewrites(const AstRewriter &rewrites,
                                      ProcedureType     *typeX,
                                      ProcedureType     *typeY)
{
    unsigned arity = typeX->getArity();

    if (arity != typeY->getArity())
        return false;

    for (unsigned i = 0; i < arity; ++i) {
        Type *argX = typeX->getArgType(i);
        Type *argY = typeY->getArgType(i);
        if (!compareTypesUsingRewrites(rewrites, argX, argY))
            return false;
    }

    return true;
}
