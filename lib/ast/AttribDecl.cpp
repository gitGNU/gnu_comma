//===-- ast/AttribDecl.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/AttribDecl.h"

using namespace comma;
using namespace comma::attrib;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// PosAD
PosAD *PosAD::create(AstResource &resource, IntegerDecl *prefixDecl)
{
    IntegerType *prefix = prefixDecl->getType();
    IdentifierInfo *name = resource.getIdentifierInfo(getAttributeString(Pos));
    Location loc = prefixDecl->getLocation();

    // S'Pos denotes a function with the following profile:
    //    function (Arg : S'Base) return universal_integer;
    IdentifierInfo *key = resource.getIdentifierInfo("arg");
    Type *argType = prefix->getBaseSubtype();
    Type *retType = UniversalType::getUniversalInteger();

    FunctionType *fnTy = resource.getFunctionType(&argType, 1, retType);

    return new PosAD(prefix, name, loc, &key, fnTy, prefixDecl);
}

PosAD *PosAD::create(AstResource &resource, EnumerationDecl *prefixDecl)
{
    EnumerationType *prefix = prefixDecl->getType();
    IdentifierInfo *name = resource.getIdentifierInfo(getAttributeString(Pos));
    Location loc = prefixDecl->getLocation();

    // S'Pos denotes a function with the following profile:
    //    function (Arg : S'Base) return universal_integer;
    IdentifierInfo *key = resource.getIdentifierInfo("arg");
    Type *argType = prefix->getBaseSubtype();
    Type *retType = UniversalType::getUniversalInteger();

    FunctionType *fnTy = resource.getFunctionType(&argType, 1, retType);

    return new PosAD(prefix, name, loc, &key, fnTy, prefixDecl);
}

//===----------------------------------------------------------------------===//
// ValAD
ValAD *ValAD::create(AstResource &resource, IntegerDecl *prefixDecl)
{
    IntegerType *prefix = prefixDecl->getType();
    IdentifierInfo *name = resource.getIdentifierInfo(getAttributeString(Val));
    Location loc = prefixDecl->getLocation();

    // S'Val denotes a function with the following profile:
    //   function (Arg : universal_integer) return S'Base
    IdentifierInfo *key = resource.getIdentifierInfo("arg");
    Type *argType = UniversalType::getUniversalInteger();
    Type *retType = prefix->getBaseSubtype();

    FunctionType *fnTy = resource.getFunctionType(&argType, 1, retType);

    return new ValAD(prefix, name, loc, &key, fnTy, prefixDecl);
}

ValAD *ValAD::create(AstResource &resource, EnumerationDecl *prefixDecl)
{
    EnumerationType *prefix = prefixDecl->getType();
    IdentifierInfo *name = resource.getIdentifierInfo(getAttributeString(Val));
    Location loc = prefixDecl->getLocation();

    // S'Val denotes a function with the following profile:
    //   function (Arg : universal_integer) return S'Base
    IdentifierInfo *key = resource.getIdentifierInfo("arg");
    Type *argType = UniversalType::getUniversalInteger();
    Type *retType = prefix->getBaseSubtype();

    FunctionType *fnTy = resource.getFunctionType(&argType, 1, retType);

    return new ValAD(prefix, name, loc, &key, fnTy, prefixDecl);
}
