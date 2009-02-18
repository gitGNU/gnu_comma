//===-- ast/Decl.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/AstRewriter.h"
#include <algorithm>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Decl

DeclarativeRegion *Decl::asDeclarativeRegion()
{
    switch (getKind()) {
    default:
        return 0;
    case AST_SignatureDecl:
        return static_cast<SignatureDecl*>(this);
    case AST_VarietyDecl:
        return static_cast<VarietyDecl*>(this);
    case AST_DomainDecl:
        return static_cast<DomainDecl*>(this);
    case AST_FunctorDecl:
        return static_cast<FunctorDecl*>(this);
    case AST_AddDecl:
        return static_cast<AddDecl*>(this);
    case AST_FunctionDecl:
        return static_cast<FunctionDecl*>(this);
    case AST_ProcedureDecl:
        return static_cast<ProcedureDecl*>(this);
    }
}

//===----------------------------------------------------------------------===//
// ModelDecl
ModelDecl::ModelDecl(AstKind kind, IdentifierInfo *percentId)
    : Decl(kind)
{
    assert(std::strcmp(percentId->getString(), "%") == 0 &&
           "Percent IdInfo not == \"%\"!");
    percent = DomainType::getPercent(percentId, this);
}

ModelDecl::ModelDecl(AstKind         kind,
                     IdentifierInfo *percentId,
                     IdentifierInfo *info,
                     Location        loc)
    : Decl(kind, info, loc)
{
    assert(std::strcmp(percentId->getString(), "%") == 0 &&
           "Percent IdInfo not == \"%\"!");
    percent = DomainType::getPercent(percentId, this);
}

//===----------------------------------------------------------------------===//
// Sigoid

Sigoid::Sigoid(AstKind kind, DomainType *percent)
    : ModelDecl(kind, percent->getIdInfo()),
      DeclarativeRegion(kind)
{
    assert(percent->denotesPercent());
}

void Sigoid::addSupersignature(SignatureType *supersignature)
{
    if (directSupers.insert(supersignature)) {
        Sigoid *superDecl = supersignature->getDeclaration();
        AstRewriter rewriter;

        // Rewrite the percent node of the super signature to that of this
        // signature.
        rewriter.addRewrite(superDecl->getPercent(), getPercent());

        // If the supplied super signature is parameterized, install rewrites
        // mapping the formal parameters of the signature to the actual
        // parameters of the type.
        if (VarietyDecl *variety = superDecl->getVariety()) {
            unsigned arity = variety->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                DomainType *formal = variety->getFormalDomain(i);
                DomainType *actual = supersignature->getActualParameter(i);
                rewriter.addRewrite(formal, actual);
            }
        }

        sig_iterator iter = superDecl->beginSupers();
        sig_iterator endIter = superDecl->endSupers();
        for ( ; iter != endIter; ++iter) {
            SignatureType *rewrite = rewriter.rewrite(*iter);
            supersignatures.insert(rewrite);
        }
        supersignatures.insert(supersignature);
    }
}

//===----------------------------------------------------------------------===//
// SignatureDecl
SignatureDecl::SignatureDecl(IdentifierInfo *percentId)
    : Sigoid(AST_SignatureDecl, percentId)
{
    canonicalType = new SignatureType(this);
}

SignatureDecl::SignatureDecl(IdentifierInfo *percentId,
                             IdentifierInfo *info,
                             const Location &loc)
    : Sigoid(AST_SignatureDecl, percentId, info, loc)
{
    canonicalType = new SignatureType(this);
}

SignatureDecl::SignatureDecl(DomainDecl *domain)
    : Sigoid(AST_SignatureDecl, domain->getPercent())
{
    canonicalType = new SignatureType(this);
    // Make the given domain this signatures declarative parent.
    setParent(domain);
}

SignatureDecl::SignatureDecl(FunctorDecl *functor)
    : Sigoid(AST_SignatureDecl, functor->getPercent())
{
    canonicalType = new SignatureType(this);
    // Make the given functor this signatures declarative parent.
    setParent(functor);
}

bool SignatureDecl::isPrincipleSignature() const
{
    const DeclarativeRegion *region = getParent();
    // FIXME:  Once signatures are always declared in their proper context
    // we should always have a parent region.
    if (region) {
        const Decl *decl = region->asDecl();
        return isa<DomainDecl>(decl) || isa<FunctorDecl>(decl);
    }
    return false;
}

//===----------------------------------------------------------------------===//
// VarietyDecl

VarietyDecl::VarietyDecl(IdentifierInfo  *percentId,
                         DomainType     **formals,
                         unsigned         arity)
    : Sigoid(AST_VarietyDecl, percentId)
{
    varietyType = new VarietyType(formals, this, arity);
}

VarietyDecl::VarietyDecl(IdentifierInfo *percentId,
                         IdentifierInfo *name,
                         Location        loc,
                         DomainType    **formals,
                         unsigned        arity)
    : Sigoid(AST_VarietyDecl, percentId, name, loc)
{
    varietyType = new VarietyType(formals, this, arity);
}

SignatureType *
VarietyDecl::getCorrespondingType(DomainType **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    SignatureType *type;

    SignatureType::Profile(id, args, numArgs);
    type = types.FindNodeOrInsertPos(id, insertPos);
    if (type) return type;

    type = new SignatureType(this, args, numArgs);
    types.InsertNode(type, insertPos);
    return type;
}

SignatureType *VarietyDecl::getCorrespondingType()
{
    VarietyType *thisType = getType();
    DomainType **formals = reinterpret_cast<DomainType**>(thisType->formals);
    return getCorrespondingType(formals, getArity());
}

//===----------------------------------------------------------------------===//
// Domoid

Domoid::Domoid(AstKind         kind,
               IdentifierInfo *percentId,
               IdentifierInfo *idInfo,
               Location        loc)
    : ModelDecl(kind, percentId, idInfo, loc) { }

//===----------------------------------------------------------------------===//
// AddDecl

// An AddDecl's declarative region is a sub-region of its parent domain decl.
AddDecl::AddDecl(DomainDecl *domain)
    : Decl(AST_AddDecl),
      DeclarativeRegion(AST_AddDecl, domain) { }

AddDecl::AddDecl(FunctorDecl *functor)
    : Decl(AST_AddDecl),
      DeclarativeRegion(AST_AddDecl, functor) { }

bool AddDecl::implementsDomain() const
{
    return isa<DomainDecl>(getParent()->asDecl());
}

bool AddDecl::implementsFunctor() const
{
    return isa<FunctorDecl>(getParent()->asDecl());
}

DomainDecl *AddDecl::getImplementedDomain()
{
    return dyn_cast<DomainDecl>(getParent()->asDecl());
}

FunctorDecl *AddDecl::getImplementedFunctor()
{
    return dyn_cast<FunctorDecl>(getParent()->asDecl());
}

//===----------------------------------------------------------------------===//
// DomainDecl
DomainDecl::DomainDecl(IdentifierInfo *percentId,
                       IdentifierInfo *name,
                       const Location &loc)
    : Domoid(AST_DomainDecl, percentId, name, loc),
      DeclarativeRegion(AST_DomainDecl)
{
    canonicalType      = new DomainType(this);
    principleSignature = new SignatureDecl(this);
    implementation     = new AddDecl(this);
}

//===----------------------------------------------------------------------===//
// FunctorDecl

FunctorDecl::FunctorDecl(IdentifierInfo *percentId,
                         IdentifierInfo *name,
                         Location        loc,
                         DomainType    **formals,
                         unsigned        arity)
    : Domoid(AST_FunctorDecl, percentId, name, loc),
      DeclarativeRegion(AST_FunctorDecl)
{
    functor            = new FunctorType(formals, this, arity);
    principleSignature = new SignatureDecl(this);
    implementation     = new AddDecl(this);
}

DomainType *
FunctorDecl::getCorrespondingType(DomainType **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    DomainType *type;

    DomainType::Profile(id, args, numArgs);
    type = types.FindNodeOrInsertPos(id, insertPos);
    if (type) return type;

    type = new DomainType(this, args, numArgs);
    types.InsertNode(type, insertPos);
    return type;
}

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
AbstractDomainDecl::AbstractDomainDecl(IdentifierInfo *name,
                                       SignatureType  *type,
                                       Location        loc)
    : Domoid(AST_AbstractDomainDecl,
             type->getDeclaration()->getPercent()->getIdInfo(), name, loc),
      DeclarativeRegion(AST_AbstractDomainDecl),
      signature(type)
{
    abstractType = new DomainType(this);

    AstRewriter rewriter;
    Sigoid     *sigoid = type->getDeclaration();

    // Establish a mapping from the % node of the signature to the abstract
    // domain type.
    rewriter.addRewrite(sigoid->getPercent(), abstractType);

    // Establish mappings from the formal parameters of the signature to the
    // actual parameters of the type (this is a no-op if the signature is not
    // parametrized).
    rewriter.installRewrites(type);

    for (Sigoid::DeclIter iter = sigoid->beginDecls();
         iter != sigoid->endDecls(); ++iter) {
        Decl *decl    = iter->second;
        Decl *newDecl = 0;

        switch (decl->getKind()) {

        default:
            assert(false && "Bad type of declaration in signature!");
            break;

        case AST_FunctionDecl: {
            FunctionDecl *fdecl = cast<FunctionDecl>(decl);
            FunctionType *ftype = rewriter.rewrite(fdecl->getType());
            newDecl = new FunctionDecl(decl->getIdInfo(), 0, ftype, this);
            break;
        }

        case AST_ProcedureDecl : {
            ProcedureDecl *pdecl = cast<ProcedureDecl>(decl);
            ProcedureType *ptype = rewriter.rewrite(pdecl->getType());
            newDecl = new ProcedureDecl(decl->getIdInfo(), 0, ptype, this);
            break;
        }
        }
        this->addDecl(newDecl);
    }
}

//===----------------------------------------------------------------------===//
// SubroutineDecl

SubroutineDecl::SubroutineDecl(AstKind            kind,
                               IdentifierInfo    *name,
                               Location           loc,
                               ParamValueDecl   **params,
                               unsigned           numParams,
                               DomainType        *returnType,
                               DeclarativeRegion *parent)
    : Decl(kind, name, loc),
      DeclarativeRegion(kind, parent),
      routineType(0),
      baseDeclaration(0),
      parameters(0),
      body(0)
{
    assert(this->denotesSubroutineDecl());

    // Create our own copy of the parameter set.
    if (numParams > 0) {
        parameters = new ParamValueDecl*[numParams];
        std::copy(params, params + numParams, parameters);
    }

    // We must construct a subroutine type for this decl.  Begin by extracting
    // the domain types and associated indentifier infos from each of the
    // parameters.
    llvm::SmallVector<DomainType*, 6>     paramTypes(numParams);
    llvm::SmallVector<IdentifierInfo*, 6> paramIds(numParams);
    for (unsigned i = 0; i < numParams; ++i) {
        ParamValueDecl *param = parameters[i];
        paramTypes[i] = param->getType();
        paramIds[i]   = param->getIdInfo();

        // Since the parameters of a subroutine are created before the
        // subroutine itself, the associated declarative region of each
        // parameter should be null.  Assert this invarient and update each
        // param to point to the new context.
        assert(!param->getDeclarativeRegion() &&
               "Parameter associated with invalid region!");
        param->setDeclarativeRegion(this);
    }

    // Construct the type of this subroutine.
    if (kind == AST_FunctionDecl)
        routineType = new FunctionType(&paramIds[0],
                                       &paramTypes[0],
                                       numParams,
                                       returnType);
    else {
        assert(!returnType && "Procedures cannot have return types!");
        routineType = new ProcedureType(&paramIds[0],
                                        &paramTypes[0],
                                        numParams);
    }

    // Set the parameter modes for the type.
    for (unsigned i = 0; i < numParams; ++i)
        routineType->setParameterMode(params[i]->getParameterMode(), i);
}

SubroutineDecl::SubroutineDecl(AstKind            kind,
                               IdentifierInfo    *name,
                               Location           loc,
                               SubroutineType    *type,
                               DeclarativeRegion *parent)
    : Decl(kind, name, loc),
      DeclarativeRegion(kind, parent),
      routineType(type),
      baseDeclaration(0),
      parameters(0),
      body(0)
{
    assert(this->denotesSubroutineDecl());

    // In this constructor, we need to create a set of ParamValueDecl nodes
    // which correspond to the supplied type.
    unsigned numParams = type->getArity();
    if (numParams > 0) {
        parameters = new ParamValueDecl*[numParams];
        for (unsigned i = 0; i < numParams; ++i) {
            IdentifierInfo *formal = type->getKeyword(i);
            DomainType *formalType = type->getArgType(i);
            ParameterMode     mode = type->getParameterMode(i);
            ParamValueDecl  *param;

            // Note that as these param decls are implicitly generated we supply
            // an invalid location for each node.
            param = new ParamValueDecl(formal, formalType, mode, 0);
            param->setDeclarativeRegion(this);
            parameters[i] = param;
        }
    }
}

void SubroutineDecl::setBaseDeclaration(SubroutineDecl *routineDecl)
{
    assert(baseDeclaration == 0 && "Cannot reset base declaration!");
    assert(((isa<FunctionDecl>(this) && isa<FunctionDecl>(routineDecl)) ||
            (isa<ProcedureDecl>(this) && isa<ProcedureDecl>(routineDecl))) &&
           "Base declarations must be of the same kind as the parent!");
    baseDeclaration = routineDecl;
}

//===----------------------------------------------------------------------===//
// ParamValueDecl

bool ParamValueDecl::parameterModeSpecified() const
{
    ParameterMode mode = static_cast<ParameterMode>(bits);
    return mode == MODE_DEFAULT;
}

ParameterMode ParamValueDecl::getParameterMode() const
{
    ParameterMode mode = static_cast<ParameterMode>(bits);
    if (mode == MODE_DEFAULT)
        return MODE_IN;
    else
        return mode;
}
