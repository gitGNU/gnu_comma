//===-- ast/Decl.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/basic/IdentifierPool.h"

using namespace comma;

//===----------------------------------------------------------------------===//
// ModelDecl
ModelDecl::ModelDecl(AstKind kind)
    : TypeDecl(kind), location(), typeComplete(false)
{
    percent = new PercentType(this);
}

ModelDecl::ModelDecl(AstKind kind, IdentifierInfo *info, const Location &loc)
    : TypeDecl(kind, info), location(loc), typeComplete(false)
{
    percent = new PercentType(this);
}

ParameterizedModel *ModelDecl::getParameterizedModel()
{
    ParameterizedModel *model;
    if (model = llvm::dyn_cast<VarietyDecl>(this))
        return model;
    else if (model = llvm::dyn_cast<FunctorDecl>(this))
        return model;
    return 0;
}

//===----------------------------------------------------------------------===//
// Sigoid
void Sigoid::addSupersignature(SignatureType *supersignature)
{
    if (directSupers.insert(supersignature)) {
        if (VarietyDecl *variety = llvm::dyn_cast<VarietyDecl>(this)) {
            VarietyDecl::type_iterator iter = variety->beginTypes();
            VarietyDecl::type_iterator endIter = variety->endTypes();
            for ( ; iter != endIter; ++iter)
                iter->addSupersignature(supersignature);
        }
        else {
            SignatureDecl *sig = llvm::dyn_cast<SignatureDecl>(this);
            assert(sig && "Not a signature or variety?!?");
            sig->getCorrespondingType()->addSupersignature(supersignature);
        }
    }
}

//===----------------------------------------------------------------------===//
// SignatureDecl
SignatureDecl::SignatureDecl()
    : Sigoid(AST_SignatureDecl)
{
    canonicalType = new SignatureType(this);
}

SignatureDecl::SignatureDecl(IdentifierInfo *info, const Location &loc)
    : Sigoid(AST_SignatureDecl, info, loc)
{
    canonicalType = new SignatureType(this);
}

SignatureType *SignatureDecl::getCorrespondingType()
{
    return canonicalType;
}

//===----------------------------------------------------------------------===//
// VarietyDecl
SignatureType *
VarietyDecl::getCorrespondingType(ModelType **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    SignatureType *type;

    SignatureType::Profile(id, args, numArgs);
    if (type = types.FindNodeOrInsertPos(id, insertPos))
        return type;

    type = new SignatureType(this, args, numArgs);
    types.InsertNode(type, insertPos);
    return type;
}

//===----------------------------------------------------------------------===//
// Domoid

Domoid::Domoid(AstKind kind,
               IdentifierInfo *idInfo,
               Location loc,
               SignatureType *signature)
    : ModelDecl(kind, idInfo, loc)
{
    if (signature)
        principleSignature = signature;
    else {
        SignatureDecl *decl = new SignatureDecl();
        principleSignature = decl->getCorrespondingType();
    }
}

//===----------------------------------------------------------------------===//
// DomainDecl
DomainDecl::DomainDecl(IdentifierInfo *name,
                       const Location &loc, SignatureType *sig)
    : Domoid(AST_DomainDecl, name, loc, sig)
{
    canonicalType = new DomainType(this);
}

DomainDecl::DomainDecl(AstKind kind, IdentifierInfo *info, Location loc)
    : Domoid(kind, info, loc)
{
    canonicalType = new DomainType(this);
}

DomainType * DomainDecl::getCorrespondingType()
{
    return canonicalType;
}

//===----------------------------------------------------------------------===//
// FunctorDecl

DomainType *
FunctorDecl::getCorrespondingType(ModelType **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    DomainType *type;

    DomainType::Profile(id, args, numArgs);
    if (type = types.FindNodeOrInsertPos(id, insertPos))
        return type;

    type = new DomainType(this, args, numArgs);
    types.InsertNode(type, insertPos);
    return type;
}
