//===-- ast/AstRewriter.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
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

Type *AstRewriter::rewriteType(Type *type) const
{
    if (Type *result = findRewrite(type))
        return result;

    switch(type->getKind()) {

    default: return type;

    case Ast::AST_FunctionType:
        return rewriteType(cast<FunctionType>(type));
    case Ast::AST_ProcedureType:
        return rewriteType(cast<ProcedureType>(type));
    }
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
    llvm::SmallVector<Type*, 16> paramTypes(arity);

    rewriteParameters(ftype, arity, paramTypes.data());
    return resource.getFunctionType(paramTypes.data(), arity, returnType);
}

ProcedureType *AstRewriter::rewriteType(ProcedureType *ptype) const
{
    unsigned arity = ptype->getArity();
    llvm::SmallVector<Type*, 16> paramTypes(arity);

    rewriteParameters(ptype, arity, paramTypes.data());
    return resource.getProcedureType(paramTypes.data(), arity);
}
