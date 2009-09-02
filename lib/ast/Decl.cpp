//===-- ast/Decl.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstRewriter.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Stmt.h"

#include <algorithm>
#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Decl

DeclRegion *Decl::asDeclRegion()
{
    switch (getKind()) {
    default:
        return 0;
    case AST_DomainInstanceDecl:
        return static_cast<DomainInstanceDecl*>(this);
    case AST_DomainDecl:
        return static_cast<DomainDecl*>(this);
    case AST_AbstractDomainDecl:
        return static_cast<AbstractDomainDecl*>(this);
    case AST_EnumerationDecl:
        return static_cast<EnumerationDecl*>(this);
    case AST_SignatureDecl:
        return static_cast<SignatureDecl*>(this);
    case AST_VarietyDecl:
        return static_cast<VarietyDecl*>(this);
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
// OverloadedDeclName

IdentifierInfo *OverloadedDeclName::getIdInfo() const {
    return decls[0]->getIdInfo();
}

void OverloadedDeclName::verify()
{
    assert(decls.size() > 1 && "OverloadedDeclName's must be overloaded!");
    IdentifierInfo *idInfo = decls[0]->getIdInfo();
    for (unsigned i = 1; i < decls.size(); ++i) {
        assert(decls[i]->getIdInfo() == idInfo &&
               "All overloads must have the same identifier!");
    }
}

//===----------------------------------------------------------------------===//
// ModelDecl

ModelDecl::ModelDecl(AstResource &resource,
                     AstKind kind, IdentifierInfo *name, Location loc)
    : Decl(kind, name, loc),
      DeclRegion(kind),
      percent(0)
{
    IdentifierInfo *percentId = resource.getIdentifierInfo("%");
    percent =  DomainType::getPercent(percentId, this);
}

bool ModelDecl::addDirectSignature(SignatureType *signature)
{
    // Rewrite % nodes of the signature to the % nodes of this model and map any
    // formal arguments to the actuals.
    AstRewriter rewrites;
    rewrites.addRewrite(signature->getSigoid()->getPercent(),
                        getPercent());
    rewrites.installRewrites(signature);
    return sigset.addDirectSignature(signature, rewrites);
}

unsigned ModelDecl::getArity() const
{
    return 0;
}

/// Returns the abstract domain declaration corresponding the i'th formal
/// parameter.  This method will assert if this declaration is not
/// parameterized.
AbstractDomainDecl *ModelDecl::getFormalDecl(unsigned i) const
{
    assert(!isParameterized() &&
           "Parameterized decls must implement this method!");
    assert(false &&
           "Cannot retrieve formal decls from a non-parameterized model!");
}

/// Returns the index of the given AbstractDomainDecl which must be a formal
/// parameter of this model.  This method will assert if this declaration is not
/// parameterized.
unsigned ModelDecl::getFormalIndex(const AbstractDomainDecl *ADDecl) const
{
    assert(!isParameterized() &&
           "Parameterized decls must implement this method!");
    assert(false &&
           "Cannot retrieve formal index from a non-parameterized model!");
}

/// Returns the type of the i'th formal formal parameter.  This method will
/// assert if this declaration is not parameterized.
DomainType *ModelDecl::getFormalType(unsigned i) const
{
    assert(!isParameterized() &&
           "Parameterized decls must implement this method!");
    assert(false &&
           "Cannot retrieve formal type from a non-parameterized model!");
}

/// Returns the SignatureType which the i'th actual parameter must satisfy.
/// This method will assert if this declaration is not parameterized.
SignatureType *ModelDecl::getFormalSignature(unsigned i) const
{
    assert(!isParameterized() &&
           "Parameterized decls must implement this method!");
    assert(false &&
           "Cannot retrieve formal signature from a non-parameterized model!");
}

/// Returns the IdentifierInfo which labels the i'th formal parameter.  This
/// method will assert if this declaration is not parameterized.
IdentifierInfo *ModelDecl::getFormalIdInfo(unsigned i) const
{
    assert(!isParameterized() &&
           "Parameterized decls must implement this method!");
    assert(false &&
           "Cannot retrieve formal identifier from a non-parameterized model!");
}

/// Returns the index of the parameter corresponding to the given keyword,
/// or -1 if no such keyword exists.  This method will assert if this
/// declaration is not parameterized.
int ModelDecl::getKeywordIndex(IdentifierInfo *keyword) const
{
    assert(!isParameterized() &&
           "Parameterized decls must implement this method!");
    assert(false &&
           "Cannot retrieve keyword index from a non-parameterized model!");
}


//===----------------------------------------------------------------------===//
// SignatureDecl
SignatureDecl::SignatureDecl(AstResource &resource,
                             IdentifierInfo *info, const Location &loc)
    : Sigoid(resource, AST_SignatureDecl, info, loc)
{
    canonicalType = new SignatureType(this);
}

//===----------------------------------------------------------------------===//
// VarietyDecl

VarietyDecl::VarietyDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         AbstractDomainDecl **formals, unsigned arity)
    : Sigoid(resource, AST_VarietyDecl, name, loc),
      arity(arity)
{
    formalDecls = new AbstractDomainDecl*[arity];
    std::copy(formals, formals + arity, formalDecls);
}

SignatureType *
VarietyDecl::getCorrespondingType(Type **args, unsigned numArgs)
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

/// Returns the index of the given AbstractDomainDecl (which must be a
/// formal parameter of this variety).
unsigned VarietyDecl::getFormalIndex(const AbstractDomainDecl *ADDecl) const
{
    for (unsigned i = 0; i < arity; ++i) {
        AbstractDomainDecl *candidate = formalDecls[i];
        if (candidate == ADDecl)
            return i;
    }
    assert(false && "Domain decl is not a formal parameter of this variety!");
    return -1U;
}

/// Returns the type of of the i'th formal parameter.
DomainType *VarietyDecl::getFormalType(unsigned i) const {
    return getFormalDecl(i)->getType();
}

/// Returns the SignatureType which the i'th actual parameter must satisfy.
SignatureType *VarietyDecl::getFormalSignature(unsigned i) const
{
    return getFormalDecl(i)->getSignatureType();
}

/// Returns the IdentifierInfo which labels the i'th formal parameter.
IdentifierInfo *VarietyDecl::getFormalIdInfo(unsigned i) const {
    return getFormalDecl(i)->getIdInfo();
}

//===----------------------------------------------------------------------===//
// Domoid

Domoid::Domoid(AstResource &resource,
               AstKind kind, IdentifierInfo *idInfo, Location loc)
    : ModelDecl(resource, kind, idInfo, loc) { }

//===----------------------------------------------------------------------===//
// AddDecl

// An AddDecl's declarative region is a sub-region of its parent domain decl.
AddDecl::AddDecl(DomainDecl *domain)
    : Decl(AST_AddDecl),
      DeclRegion(AST_AddDecl, domain),
      carrier(0) { }

AddDecl::AddDecl(FunctorDecl *functor)
    : Decl(AST_AddDecl),
      DeclRegion(AST_AddDecl, functor),
      carrier(0) { }

bool AddDecl::implementsDomain() const
{
    return isa<DomainDecl>(getParent()->asAst());
}

bool AddDecl::implementsFunctor() const
{
    return isa<FunctorDecl>(getParent()->asAst());
}

Domoid *AddDecl::getImplementedDomoid()
{
    return cast<Domoid>(getParent()->asAst());
}

DomainDecl *AddDecl::getImplementedDomain()
{
    return dyn_cast<DomainDecl>(getParent()->asAst());
}

FunctorDecl *AddDecl::getImplementedFunctor()
{
    return dyn_cast<FunctorDecl>(getParent()->asAst());
}

//===----------------------------------------------------------------------===//
// DomainDecl

DomainDecl::DomainDecl(AstResource &resource,
                       IdentifierInfo *name, const Location &loc)
    : Domoid(resource, AST_DomainDecl, name, loc),
      instance(0)
{
    implementation = new AddDecl(this);
}

// FIXME: This is a temporary solution to the problem of initializing domain
// instances before the corresponding DomDecl is fully initialized.  In
// particular, we need a machanism similar to the observer failcility in
// DeclRegion but for signature sets.
DomainInstanceDecl *DomainDecl::getInstance()
{
    if (instance == 0)
        instance = new DomainInstanceDecl(this, getLocation());
    return instance;
}

//===----------------------------------------------------------------------===//
// FunctorDecl

FunctorDecl::FunctorDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         AbstractDomainDecl **formals, unsigned arity)
    : Domoid(resource, AST_FunctorDecl, name, loc),
      arity(arity)
{
    formalDecls = new AbstractDomainDecl*[arity];
    std::copy(formals, formals + arity, formalDecls);

    implementation = new AddDecl(this);
}

DomainInstanceDecl *
FunctorDecl::getInstance(Type **args, unsigned numArgs, Location loc)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    DomainInstanceDecl *instance;

    DomainInstanceDecl::Profile(id, args, numArgs);
    instance = instances.FindNodeOrInsertPos(id, insertPos);
    if (instance) return instance;

    instance = new DomainInstanceDecl(this, args, numArgs, loc);
    instances.InsertNode(instance, insertPos);
    return instance;
}

/// Returns the index of the given AbstractDomainDecl (which must be a
/// formal parameter of this functor).
unsigned FunctorDecl::getFormalIndex(const AbstractDomainDecl *ADDecl) const
{
    for (unsigned i = 0; i < arity; ++i) {
        AbstractDomainDecl *candidate = formalDecls[i];
        if (candidate == ADDecl)
            return i;
    }
    assert(false && "Domain decl is not a formal parameter of this variety!");
    return -1U;
}

/// Returns the type of of the i'th formal parameter.
DomainType *FunctorDecl::getFormalType(unsigned i) const {
    return getFormalDecl(i)->getType();
}

/// Returns the SignatureType which the i'th actual parameter must satisfy.
SignatureType *FunctorDecl::getFormalSignature(unsigned i) const
{
    return getFormalDecl(i)->getSignatureType();
}

/// Returns the IdentifierInfo which labels the i'th formal parameter.
IdentifierInfo *FunctorDecl::getFormalIdInfo(unsigned i) const {
    return getFormalDecl(i)->getIdInfo();
}

//===----------------------------------------------------------------------===//
// SubroutineDecl

SubroutineDecl::SubroutineDecl(AstKind          kind,
                               IdentifierInfo  *name,
                               Location         loc,
                               ParamValueDecl **params,
                               unsigned         numParams,
                               Type            *returnType,
                               DeclRegion      *parent)
    : Decl(kind, name, loc),
      DeclRegion(kind, parent),
      immediate(false),
      opID(PO::NotPrimitive),
      routineType(0),
      parameters(0),
      body(0),
      definingDeclaration(0),
      origin(0)
{
    assert(this->denotesSubroutineDecl());

    setDeclRegion(parent);

    // Initialize our copy of the parameter set.
    if (numParams > 0) {
        parameters = new ParamValueDecl*[numParams];
        std::copy(params, params + numParams, parameters);
    }

    // We must construct a subroutine type for this decl.  Begin by extracting
    // the domain types and associated indentifier infos from each of the
    // parameters.
    llvm::SmallVector<Type*, 6> paramTypes(numParams);
    llvm::SmallVector<IdentifierInfo*, 6> paramIds(numParams);
    for (unsigned i = 0; i < numParams; ++i) {
        ParamValueDecl *param = parameters[i];
        paramTypes[i] = param->getType();
        paramIds[i] = param->getIdInfo();

        // Since the parameters of a subroutine are created before the
        // subroutine itself, the associated declarative region of each
        // parameter should be null.  Assert this invarient and update each
        // param to point to the new context.
        assert(!param->getDeclRegion() &&
               "Parameter associated with invalid region!");
        param->setDeclRegion(this);
    }

    // Construct the type of this subroutine.
    if (kind == AST_FunctionDecl || kind == AST_EnumLiteral)
        routineType = new FunctionType(paramIds.data(),
                                       paramTypes.data(),
                                       numParams,
                                       returnType);
    else {
        assert(!returnType && "Procedures cannot have return types!");
        routineType = new ProcedureType(paramIds.data(),
                                        paramTypes.data(),
                                        numParams);
    }

    // Set the parameter modes for the type.
    for (unsigned i = 0; i < numParams; ++i) {
        PM::ParameterMode mode = params[i]->getExplicitParameterMode();
        routineType->setParameterMode(mode, i);
    }
}

SubroutineDecl::SubroutineDecl(AstKind         kind,
                               IdentifierInfo *name,
                               Location        loc,
                               SubroutineType *type,
                               DeclRegion     *parent)
    : Decl(kind, name, loc),
      DeclRegion(kind, parent),
      immediate(false),
      opID(PO::NotPrimitive),
      routineType(type),
      parameters(0),
      body(0),
      definingDeclaration(0),
      origin(0)
{
    assert(this->denotesSubroutineDecl());

    setDeclRegion(parent);

    // In this constructor, we need to create a set of ParamValueDecl nodes
    // which correspond to the supplied type.
    unsigned numParams = type->getArity();
    if (numParams > 0) {
        parameters = new ParamValueDecl*[numParams];
        for (unsigned i = 0; i < numParams; ++i) {
            IdentifierInfo *formal = type->getKeyword(i);
            Type *formalType = type->getArgType(i);
            PM::ParameterMode mode = type->getExplicitParameterMode(i);
            ParamValueDecl *param;

            // Note that as these param decls are implicitly generated we supply
            // an invalid location for each node.
            param = new ParamValueDecl(formal, formalType, mode, 0);
            param->setDeclRegion(this);
            parameters[i] = param;
        }
    }
}

void SubroutineDecl::setDefiningDeclaration(SubroutineDecl *routineDecl)
{
    assert(definingDeclaration == 0 && "Cannot reset base declaration!");
    assert(((isa<FunctionDecl>(this) && isa<FunctionDecl>(routineDecl)) ||
            (isa<ProcedureDecl>(this) && isa<ProcedureDecl>(routineDecl))) &&
           "Defining declarations must be of the same kind as the parent!");
    definingDeclaration = routineDecl;
}

PM::ParameterMode SubroutineDecl::getParamMode(unsigned i) {
    return getParam(i)->getParameterMode();
}

bool SubroutineDecl::hasBody() const
{
    return body || (definingDeclaration && definingDeclaration->body);
}

BlockStmt *SubroutineDecl::getBody()
{
    if (body)
        return body;

    if (definingDeclaration)
        return definingDeclaration->body;

    return 0;
}

SubroutineDecl *SubroutineDecl::resolveOrigin()
{
    SubroutineDecl *res = this;

    while (res->hasOrigin())
        res = res->getOrigin();

    return res;
}

const SubroutineDecl *SubroutineDecl::resolveOrigin() const
{
    const SubroutineDecl *res = this;

    while (res->hasOrigin())
        res = res->getOrigin();

    return res;
}

//===----------------------------------------------------------------------===//
// DomainValueDecl

// FIXME:  Perhaps the sensible parent of the declarative region would be the
// parent of the defining declaration.
DomainValueDecl::DomainValueDecl(AstKind kind, IdentifierInfo *name, Location loc)
    : ValueDecl(kind, name, 0, loc),
      DeclRegion(kind)
{
    assert(this->denotesDomainValue());
    // Create a new unique type to represent us.
    correspondingType = new DomainType(this);
}

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
AbstractDomainDecl::AbstractDomainDecl(IdentifierInfo *name,
                                       SignatureType *sigType, Location loc)
    : DomainValueDecl(AST_AbstractDomainDecl, name, loc)
{
    AstRewriter rewriter;
    Sigoid *sigoid = sigType->getSigoid();

    // Establish a mapping from the % node of the signature to the type of this
    // abstract domain.
    rewriter.addRewrite(sigoid->getPercent(), getType());

    // Establish mappings from the formal parameters of the signature to the
    // actual parameters of the type (this is a no-op if the signature is not
    // parametrized).
    rewriter.installRewrites(getType());

    // Rewrite the declarations proided by the signature and populated our
    // region with the results.
    addDeclarationsUsingRewrites(rewriter, sigoid);

    // Add our rewritten signature hierarchy.
    sigset.addDirectSignature(sigType, rewriter);
}

//===----------------------------------------------------------------------===//
// DomainInstanceDecl
DomainInstanceDecl::DomainInstanceDecl(DomainDecl *domain, Location loc)
    : DomainValueDecl(AST_DomainInstanceDecl, domain->getIdInfo(), loc),
      definition(domain)
{
    // Ensure that we are notified if the declarations provided by the defining
    // domoid change.
    domain->addObserver(this);

    AstRewriter rewriter;
    rewriter.addRewrite(domain->getPercent(), getType());
    rewriter.installRewrites(getType());
    addDeclarationsUsingRewrites(rewriter, domain);

    // Populate our signature set with a rewritten version of our defining
    // domain.
    const SignatureSet &SS = domain->getSignatureSet();
    for (SignatureSet::const_iterator I = SS.begin(); I != SS.end(); ++I)
        sigset.addDirectSignature(*I, rewriter);
}

DomainInstanceDecl::DomainInstanceDecl(FunctorDecl *functor,
                                       Type **args, unsigned numArgs,
                                       Location loc)
    : DomainValueDecl(AST_DomainInstanceDecl, functor->getIdInfo(), loc),
      definition(functor)
{
    assert(functor->getArity() == numArgs &&
           "Wrong number of arguments for domain instance!");

    arguments = new Type*[numArgs];
    std::copy(args, args + numArgs, arguments);

    // Ensure that we are notified if the declarations provided by the defining
    // functor change.
    functor->addObserver(this);

    AstRewriter rewriter;
    rewriter.addRewrite(functor->getPercent(), getType());
    rewriter.installRewrites(getType());
    addDeclarationsUsingRewrites(rewriter, functor);

    // Populate our signature set with a rewritten version of our defining
    // functor.
    const SignatureSet &SS = functor->getSignatureSet();
    for (SignatureSet::const_iterator I = SS.begin(); I != SS.end(); ++I)
        sigset.addDirectSignature(*I, rewriter);
}

bool DomainInstanceDecl::isDependent() const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        DomainType *param = cast<DomainType>(getActualParameter(i));
        if (param->isAbstract())
            return true;
        if (param->denotesPercent())
            continue;
        if (param->getInstanceDecl()->isDependent())
            return true;
    }
    return false;
}

unsigned DomainInstanceDecl::getArity() const
{
    if (getType()->denotesPercent())
        return 0;
    if (FunctorDecl *functor = dyn_cast<FunctorDecl>(definition))
        return functor->getArity();
    return 0;
}

void DomainInstanceDecl::notifyAddDecl(Decl *decl)
{
    AstRewriter rewriter;
    rewriter.addRewrite(getDefinition()->getPercent(), getType());
    rewriter.installRewrites(getType());
    addDeclarationUsingRewrites(rewriter, decl);
}

void DomainInstanceDecl::notifyRemoveDecl(Decl *decl)
{
    // FIXME:  Implement.
}

void DomainInstanceDecl::Profile(llvm::FoldingSetNodeID &id,
                                 Type **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// ParamValueDecl

PM::ParameterMode ParamValueDecl::getExplicitParameterMode() const
{
    return static_cast<PM::ParameterMode>(bits);
}

bool ParamValueDecl::parameterModeSpecified() const
{
    return getExplicitParameterMode() == PM::MODE_DEFAULT;
}

PM::ParameterMode ParamValueDecl::getParameterMode() const
{
    PM::ParameterMode mode = getExplicitParameterMode();
    if (mode == PM::MODE_DEFAULT)
        return PM::MODE_IN;
    else
        return mode;
}

//===----------------------------------------------------------------------===//
// EnumLiteral
EnumLiteral::EnumLiteral(EnumerationDecl *decl,
                         IdentifierInfo  *name,
                         Location         loc)
    : FunctionDecl(AST_EnumLiteral, name, loc, 0, 0, decl->getType(), decl)
{
    // Add ourselves to the enclosing EnumerationDecl, and mark this new
    // function-like declaration as primitive.
    index = decl->getNumLiterals();
    decl->addDecl(this);
    setAsPrimitive(PO::EnumFunction);
}

//===----------------------------------------------------------------------===//
// EnumerationDecl
EnumerationDecl::EnumerationDecl(IdentifierInfo *name,
                                 Location        loc,
                                 DeclRegion     *parent)
    : TypedDecl(AST_EnumerationDecl, name, loc),
      DeclRegion(AST_EnumerationDecl, parent),
      numLiterals(0)
{
    setDeclRegion(parent);
    correspondingType = new EnumerationType(this);

    // Ensure that each call to addDecl notifies us so that we can keep track of
    // each enumeration literal added to this decl.
    addObserver(this);
}

void EnumerationDecl::notifyAddDecl(Decl *decl)
{
    if (isa<EnumLiteral>(decl))
        numLiterals++;
}

void EnumerationDecl::notifyRemoveDecl(Decl *decl)
{
    if (isa<EnumLiteral>(decl))
        numLiterals--;
}

EnumLiteral *EnumerationDecl::findLiteral(IdentifierInfo *name)
{
    PredRange range = findDecls(name);

    if (range.first != range.second)
        return cast<EnumLiteral>(*range.first);
    return 0;
}

//===----------------------------------------------------------------------===//
// IntegerDecl

IntegerDecl::IntegerDecl(IdentifierInfo *name, Location loc,
                         Expr *lowRange, Expr *highRange,
                         IntegerType *baseType, DeclRegion *parent)
    : TypedDecl(AST_IntegerDecl, name, loc),
      DeclRegion(AST_IntegerDecl, parent),
      lowExpr(lowRange), highExpr(highRange)
{
    correspondingType = new TypedefType(baseType, this);
}

IntegerDecl::~IntegerDecl()
{
    delete correspondingType;
}
