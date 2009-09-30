//===-- ast/Decl.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstRewriter.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Stmt.h"

#include <algorithm>
#include <cstring>
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
    case AST_AbstractDomainDecl:
        return static_cast<AbstractDomainDecl*>(this);
    case AST_PercentDecl:
        return static_cast<PercentDecl*>(this);
    case AST_EnumerationDecl:
        return static_cast<EnumerationDecl*>(this);
    case AST_AddDecl:
        return static_cast<AddDecl*>(this);
    case AST_FunctionDecl:
        return static_cast<FunctionDecl*>(this);
    case AST_ProcedureDecl:
        return static_cast<ProcedureDecl*>(this);
    case AST_IntegerDecl:
        return static_cast<IntegerDecl*>(this);
    }
}

//===----------------------------------------------------------------------===//
// ModelDecl

ModelDecl::ModelDecl(AstResource &resource,
                     AstKind kind, IdentifierInfo *name, Location loc)
    : Decl(kind, name, loc),
      percent(0),
      resource(resource)
{
    percent = new PercentDecl(resource, this);
}

ModelDecl::~ModelDecl()
{
    delete percent;
}

const SignatureSet &ModelDecl::getSignatureSet() const
{
    return percent->getSignatureSet();
}

bool ModelDecl::addDirectSignature(SigInstanceDecl *signature)
{
    // Rewrite % nodes of the signature to the % nodes of this model and map any
    // formal arguments to the actuals.
    AstRewriter rewrites(resource);
    rewrites.addRewrite(signature->getSigoid()->getPercentType(),
                        getPercentType());
    rewrites.installRewrites(signature);
    return percent->sigset.addDirectSignature(signature, rewrites);
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
    assert(isParameterized() &&
           "Cannot retrieve formal index from a non-parameterized model!");
    unsigned arity = getArity();
    for (unsigned i = 0; i < arity; ++i) {
        AbstractDomainDecl *candidate = getFormalDecl(i);
        if (candidate == ADDecl)
            return i;
    }
    assert(false && "Not a formal parameter decl!");
    return -1U;
}

/// Returns the type of the i'th formal formal parameter.  This method will
/// assert if this declaration is not parameterized.
DomainType *ModelDecl::getFormalType(unsigned i) const
{
    assert(isParameterized() &&
           "Cannot retrieve formal type from a non-parameterized model!");
    return getFormalDecl(i)->getType();
}

/// Returns the SigInstanceDecl which the i'th actual parameter must satisfy.
/// This method will assert if this declaration is not parameterized.
SigInstanceDecl *ModelDecl::getFormalSignature(unsigned i) const
{
    assert(isParameterized() &&
           "Cannot retrieve formal signature from a non-parameterized model!");
    return getFormalDecl(i)->getPrincipleSignature();
}

/// Returns the IdentifierInfo which labels the i'th formal parameter.  This
/// method will assert if this declaration is not parameterized.
IdentifierInfo *ModelDecl::getFormalIdInfo(unsigned i) const
{
    assert(isParameterized() &&
           "Cannot retrieve formal identifier from a non-parameterized model!");
    return getFormalDecl(i)->getIdInfo();
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

int ModelDecl::getKeywordIndex(KeywordSelector *key) const
{
    return getKeywordIndex(key->getKeyword());
}

//===----------------------------------------------------------------------===//
// SignatureDecl
SignatureDecl::SignatureDecl(AstResource &resource,
                             IdentifierInfo *info, const Location &loc)
    : Sigoid(resource, AST_SignatureDecl, info, loc)
{
    theInstance = new SigInstanceDecl(this);
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

SigInstanceDecl *
VarietyDecl::getInstance(DomainTypeDecl **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    SigInstanceDecl *instance;

    SigInstanceDecl::Profile(id, args, numArgs);
    instance = instances.FindNodeOrInsertPos(id, insertPos);
    if (instance)
        return instance;

    instance = new SigInstanceDecl(this, args, numArgs);
    instances.InsertNode(instance, insertPos);
    return instance;
}

//===----------------------------------------------------------------------===//
// Domoid

Domoid::Domoid(AstResource &resource,
               AstKind kind, IdentifierInfo *idInfo, Location loc)
    : ModelDecl(resource, kind, idInfo, loc) { }

//===----------------------------------------------------------------------===//
// AddDecl

// An AddDecl's declarative region is a sub-region of its parent domains percent
// node.
AddDecl::AddDecl(DomainDecl *domain)
    : Decl(AST_AddDecl),
      DeclRegion(AST_AddDecl, domain->getPercent()),
      carrier(0) { }

AddDecl::AddDecl(FunctorDecl *functor)
    : Decl(AST_AddDecl),
      DeclRegion(AST_AddDecl, functor->getPercent()),
      carrier(0) { }

Domoid *AddDecl::getImplementedDomoid()
{
    PercentDecl *percent = cast<PercentDecl>(getParent()->asAst());
    return cast<Domoid>(percent->getDefinition());
}

DomainDecl *AddDecl::getImplementedDomain()
{
    PercentDecl *percent = cast<PercentDecl>(getParent()->asAst());
    return dyn_cast<DomainDecl>(percent->getDefinition());
}

FunctorDecl *AddDecl::getImplementedFunctor()
{
    PercentDecl *percent = cast<PercentDecl>(getParent()->asAst());
    return dyn_cast<FunctorDecl>(percent->getDefinition());
}

bool AddDecl::implementsDomain() const
{
    const PercentDecl *percent = cast<PercentDecl>(getParent()->asAst());
    return isa<DomainDecl>(percent->getDefinition());
}

bool AddDecl::implementsFunctor() const
{
    const PercentDecl *percent = cast<PercentDecl>(getParent()->asAst());
    return isa<FunctorDecl>(percent->getDefinition());
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
        instance = new DomainInstanceDecl(this);
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
    assert(arity && "Cannot construct functors with no arguments!");

    formalDecls = new AbstractDomainDecl*[arity];
    std::copy(formals, formals + arity, formalDecls);
    implementation = new AddDecl(this);
}

DomainInstanceDecl *
FunctorDecl::getInstance(DomainTypeDecl **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    DomainInstanceDecl *instance;

    DomainInstanceDecl::Profile(id, args, numArgs);
    instance = instances.FindNodeOrInsertPos(id, insertPos);
    if (instance) return instance;

    instance = new DomainInstanceDecl(this, args, numArgs);
    instances.InsertNode(instance, insertPos);
    return instance;
}

//===----------------------------------------------------------------------===//
// SigInstanceDecl

SigInstanceDecl::SigInstanceDecl(SignatureDecl *decl)
    : Decl(AST_SigInstanceDecl, decl->getIdInfo()),
      underlyingSigoid(decl),
      arguments(0)
{ }

SigInstanceDecl::SigInstanceDecl(VarietyDecl *decl,
                                 DomainTypeDecl **args, unsigned numArgs)
    : Decl(AST_SigInstanceDecl, decl->getIdInfo()),
      underlyingSigoid(decl)
{
    assert(numArgs && "No arguments given to parameterized instance!");
    arguments = new DomainTypeDecl*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

SignatureDecl *SigInstanceDecl::getSignature() const
{
    return dyn_cast<SignatureDecl>(underlyingSigoid);
}

VarietyDecl *SigInstanceDecl::getVariety() const
{
    return dyn_cast<VarietyDecl>(underlyingSigoid);
}

unsigned SigInstanceDecl::getArity() const
{
    VarietyDecl *variety = getVariety();
    if (variety)
        return variety->getArity();
    return 0;
}

void SigInstanceDecl::Profile(llvm::FoldingSetNodeID &ID,
                              DomainTypeDecl **args, unsigned numArgs)
{
    if (numArgs == 0)
        ID.AddPointer(0);
    else {
        for (unsigned i = 0; i < numArgs; ++i)
            ID.AddPointer(args[i]);
    }
}

//===----------------------------------------------------------------------===//
// SubroutineDecl

SubroutineDecl::SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                               ParamValueDecl **params, unsigned numParams,
                               DeclRegion *parent)
    : Decl(kind, name, loc, parent),
      DeclRegion(kind, parent),
      immediate(false),
      opID(PO::NotPrimitive),
      numParameters(numParams),
      parameters(0),
      body(0),
      definingDeclaration(0),
      origin(0),
      overriddenDecl(0)
{
    assert(this->denotesSubroutineDecl());

    if (numParams > 0)
        parameters = new ParamValueDecl*[numParams];
    llvm::SmallVector<const Type*, 8> paramTypes;

    for (unsigned i = 0; i < numParams; ++i) {
        ParamValueDecl *paramDecl = params[i];
        parameters[i] = paramDecl;
        paramTypes.push_back(paramDecl->getType());
    }
}

SubroutineDecl::SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                               IdentifierInfo **keywords, SubroutineType *type,
                               DeclRegion *parent)
    : Decl(kind, name, loc, parent),
      DeclRegion(kind, parent),
      immediate(false),
      opID(PO::NotPrimitive),
      numParameters(type->getArity()),
      parameters(0),
      body(0),
      definingDeclaration(0),
      origin(0),
      overriddenDecl(0)
{
    assert(this->denotesSubroutineDecl());

    if (numParameters == 0)
        return;

    parameters = new ParamValueDecl*[numParameters];
    for (unsigned i = 0; i < numParameters; ++i) {
        Type *paramType = type->getArgType(i);
        ParamValueDecl *param =
            new ParamValueDecl(keywords[i], paramType, PM::MODE_DEFAULT, 0);
        parameters[i] = param;
    }
}

SubroutineDecl::~SubroutineDecl()
{
    if (parameters) {
        for (unsigned i = 0; i < numParameters; ++i)
            delete parameters[i];
        delete[] parameters;
    }
}

int SubroutineDecl::getKeywordIndex(IdentifierInfo *key) const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        if (parameters[i]->getIdInfo() == key)
            return i;
    }
    return -1;
}

int SubroutineDecl::getKeywordIndex(KeywordSelector *key) const
{
    return getKeywordIndex(key->getKeyword());
}

bool SubroutineDecl::keywordsMatch(const SubroutineDecl *SRDecl) const
{
    unsigned arity = getArity();
    if (SRDecl->getArity() == arity) {
        for (unsigned i = 0; i < arity; ++i)
            if (getParamKeyword(i) != SRDecl->getParamKeyword(i))
                return false;
        return true;
    }
    return false;
}

bool SubroutineDecl::paramModesMatch(const SubroutineDecl *SRDecl) const
{
    unsigned arity = getArity();
    if (SRDecl->getArity() == arity) {
        for (unsigned i = 0; i < arity; ++i)
            if (getParamMode(i) != SRDecl->getParamMode(i))
                return false;
        return true;
    }
    return false;
}

void SubroutineDecl::setDefiningDeclaration(SubroutineDecl *routineDecl)
{
    assert(definingDeclaration == 0 && "Cannot reset base declaration!");
    assert(((isa<FunctionDecl>(this) && isa<FunctionDecl>(routineDecl)) ||
            (isa<ProcedureDecl>(this) && isa<ProcedureDecl>(routineDecl))) &&
           "Defining declarations must be of the same kind as the parent!");
    definingDeclaration = routineDecl;
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

void SubroutineDecl::setOverriddenDecl(SubroutineDecl *decl) {
    assert(overriddenDecl == 0 && "Cannot reset overridden decl!");
    assert(this->getKind() == decl->getKind() &&
           "Kind mismatch for overriding decl!");
    overriddenDecl = decl;
}

//===----------------------------------------------------------------------===//
// ProcedureDecl

ProcedureDecl::ProcedureDecl(AstResource &resource,
                             IdentifierInfo *name, Location loc,
                             ParamValueDecl **params, unsigned numParams,
                             DeclRegion *parent)
    : SubroutineDecl(AST_ProcedureDecl, name, loc,
                     params, numParams, parent)
{
    // Construct our type.
    llvm::SmallVector<Type*, 8> paramTypes;
    for (unsigned i = 0; i < numParams; ++i)
        paramTypes.push_back(params[i]->getType());
    correspondingType =
        resource.getProcedureType(paramTypes.data(), numParams);
}

//===----------------------------------------------------------------------===//
// FunctionDecl

FunctionDecl::FunctionDecl(AstResource &resource,
                           IdentifierInfo *name, Location loc,
                           ParamValueDecl **params, unsigned numParams,
                           Type *returnType, DeclRegion *parent)
    : SubroutineDecl(AST_FunctionDecl, name, loc,
                     params, numParams, parent)
{
    initializeCorrespondingType(resource, returnType);
}

FunctionDecl::FunctionDecl(AstKind kind, AstResource &resource,
                           IdentifierInfo *name, Location loc,
                           ParamValueDecl **params, unsigned numParams,
                           Type *returnType, DeclRegion *parent)
    : SubroutineDecl(kind, name, loc, params, numParams, parent)
{
    initializeCorrespondingType(resource, returnType);
}

void FunctionDecl::initializeCorrespondingType(AstResource &resource,
                                               Type *returnType)
{
    llvm::SmallVector<Type*, 8> paramTypes;
    for (unsigned i = 0; i < numParameters; ++i)
        paramTypes.push_back(parameters[i]->getType());
    correspondingType =
        resource.getFunctionType(paramTypes.data(), numParameters, returnType);
}

//===----------------------------------------------------------------------===//
// DomainTypeDecl

DomainTypeDecl::DomainTypeDecl(AstKind kind, IdentifierInfo *name, Location loc)
    : TypeDecl(kind, name, loc),
      DeclRegion(kind)
{
    assert(this->denotesDomainTypeDecl());
    CorrespondingType = new DomainType(this);
}

//===----------------------------------------------------------------------===//
// AbstractDomainDecl

bool AbstractDomainDecl::addSuperSignature(SigInstanceDecl *sig)
{
    Sigoid *sigoid = sig->getSigoid();
    AstRewriter rewriter(sigoid->getAstResource());

    // Establish a mapping from the % node of the signature to the type of this
    // abstract domain.
    rewriter.addRewrite(sigoid->getPercentType(), getType());

    // Establish mappings from the formal parameters of the signature to the
    // actual parameters of the type (this is a no-op if the signature is not
    // parametrized).
    rewriter.installRewrites(getType());

    // Add our rewritten signature hierarchy.
    return sigset.addDirectSignature(sig, rewriter);
}

//===----------------------------------------------------------------------===//
// DomainInstanceDecl
DomainInstanceDecl::DomainInstanceDecl(DomainDecl *domain)
    : DomainTypeDecl(AST_DomainInstanceDecl, domain->getIdInfo()),
      definition(domain)
{
    PercentDecl *percent = domain->getPercent();

    // Ensure that we are notified if the declarations provided by the percent
    // node of the defining domoid change.
    percent->addObserver(this);

    AstRewriter rewriter(domain->getAstResource());
    rewriter.addRewrite(domain->getPercentType(), getType());
    rewriter.installRewrites(getType());
    addDeclarationsUsingRewrites(rewriter, percent);

    // Populate our signature set with a rewritten version of our defining
    // domain.
    const SignatureSet &SS = domain->getSignatureSet();
    for (SignatureSet::const_iterator I = SS.begin(); I != SS.end(); ++I)
        sigset.addDirectSignature(*I, rewriter);
}

DomainInstanceDecl::DomainInstanceDecl(FunctorDecl *functor,
                                       DomainTypeDecl **args, unsigned numArgs)
    : DomainTypeDecl(AST_DomainInstanceDecl, functor->getIdInfo()),
      definition(functor)
{
    assert(functor->getArity() == numArgs &&
           "Wrong number of arguments for domain instance!");

    arguments = new DomainTypeDecl*[numArgs];
    std::copy(args, args + numArgs, arguments);

    // Ensure that we are notified if the declarations provided by percent
    // change.
    PercentDecl *percent = functor->getPercent();
    percent->addObserver(this);

    AstRewriter rewriter(functor->getAstResource());
    rewriter.addRewrite(functor->getPercentType(), getType());
    rewriter.installRewrites(getType());
    addDeclarationsUsingRewrites(rewriter, percent);

    // Populate our signature set with a rewritten version of our defining
    // functor.
    const SignatureSet &SS = functor->getSignatureSet();
    for (SignatureSet::const_iterator I = SS.begin(); I != SS.end(); ++I)
        sigset.addDirectSignature(*I, rewriter);
}

bool DomainInstanceDecl::isDependent() const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        DomainType *param = cast<DomainType>(getActualParamType(i));
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
    AstRewriter rewriter(getDefinition()->getAstResource());
    rewriter.addRewrite(getDefinition()->getPercentType(), getType());
    rewriter.installRewrites(getType());
    addDeclarationUsingRewrites(rewriter, decl);
}

void DomainInstanceDecl::notifyRemoveDecl(Decl *decl)
{
    // FIXME:  Implement.
}

void DomainInstanceDecl::Profile(llvm::FoldingSetNodeID &id,
                                 DomainTypeDecl **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// PercentDecl

PercentDecl::PercentDecl(AstResource &resource, ModelDecl *model)
    : DomainTypeDecl(AST_PercentDecl,
                     resource.getIdentifierInfo("%"), model->getLocation()),
      underlyingModel(model) { }

//===----------------------------------------------------------------------===//
// ParamValueDecl

PM::ParameterMode ParamValueDecl::getExplicitParameterMode() const
{
    return static_cast<PM::ParameterMode>(bits);
}

void ParamValueDecl::setParameterMode(PM::ParameterMode mode)
{
    bits = mode;
}

bool ParamValueDecl::parameterModeSpecified() const
{
    return getExplicitParameterMode() != PM::MODE_DEFAULT;
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
EnumLiteral::EnumLiteral(AstResource &resource,
                         IdentifierInfo *name, Location loc, unsigned index,
                         EnumerationDecl *parent)
    : FunctionDecl(AST_EnumLiteral, resource,
                   name, loc, 0, 0, parent->getType(), parent),
      index(index)
{
    setAsPrimitive(PO::ENUM_op);
}

//===----------------------------------------------------------------------===//
// EnumerationDecl
EnumerationDecl::EnumerationDecl(AstResource &resource,
                                 IdentifierInfo *name, Location loc,
                                 std::pair<IdentifierInfo*, Location> *elems,
                                 unsigned numElems, DeclRegion *parent)
    : TypeDecl(AST_EnumerationDecl, name, loc),
      DeclRegion(AST_EnumerationDecl, parent),
      numLiterals(numElems)
{
    setDeclRegion(parent);

    // First, build the type corresponding to this declaration.  Currently,
    // enumeration types are defined only with respect to the number of
    // underlying literals.
    EnumerationType *base = resource.createEnumType(numElems);

    // Create the first named subtype of this decl.
    CorrespondingType = resource.createEnumSubType(name, base);

    // Construct enumeration literals for each Id/Location pair and add them to
    // this decls declarative region.
    for (unsigned i = 0; i < numElems; ++i) {
        IdentifierInfo *name = elems[i].first;
        Location loc = elems[i].second;

        EnumLiteral *elem = new EnumLiteral(resource, name, loc, i, this);
        addDecl(elem);
    }
}

void EnumerationDecl::generateImplicitDeclarations(AstResource &resource)
{
    EnumSubType *type = getType();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::EQ_op, loc, type, this));
}

EnumLiteral *EnumerationDecl::findLiteral(IdentifierInfo *name)
{
    PredRange range = findDecls(name);

    if (range.first != range.second)
        return cast<EnumLiteral>(*range.first);
    return 0;
}

const EnumLiteral *EnumerationDecl::findCharacterLiteral(char ch) const
{
    char target[] = { '\'', ch, '\'', 0 };

    // Traverse the declarative region and do a case by case comparison of the
    // literal names and the target string.
    for (ConstDeclIter I = beginDecls(); I != endDecls(); ++I) {
        if (EnumLiteral *lit = dyn_cast<EnumLiteral>(*I)) {
            const char *name = lit->getIdInfo()->getString();
            if (strcmp(name, target) == 0)
                return lit;
        }
    }
    return 0;
}

//===----------------------------------------------------------------------===//
// IntegerDecl

IntegerDecl::IntegerDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         Expr *lowRange, Expr *highRange,
                         const llvm::APInt &lowVal, const llvm::APInt &highVal,
                         DeclRegion *parent)
    : TypeDecl(AST_IntegerDecl, name, loc),
      DeclRegion(AST_IntegerDecl, parent),
      lowExpr(lowRange), highExpr(highRange)
{
    IntegerType *base = resource.createIntegerType(this, lowVal, highVal);
    CorrespondingType =
        resource.createIntegerSubType(name, base, lowVal, highVal);
}

// Note that we could perform these initializations in the constructor, but it
// would cause difficulties when the language primitive types are first being
// declared.  For now this is a separate method which called separately.
void IntegerDecl::generateImplicitDeclarations(AstResource &resource)
{
    IntegerSubType *type = getBaseSubType();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::EQ_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::GT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::GE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::ADD_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::SUB_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::MUL_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::POW_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::NEG_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::POS_op, loc, type, this));
}

//===----------------------------------------------------------------------===//
// ArrayDecl
ArrayDecl::ArrayDecl(AstResource &resource,
                     IdentifierInfo *name, Location loc,
                     unsigned rank, SubType **indices,
                     Type *component, bool isConstrained, DeclRegion *parent)
    : TypeDecl(AST_ArrayDecl, name, loc),
      DeclRegion(AST_ArrayDecl, parent)
{
    assert(rank != 0 && "Missing indices!");

    // Ensure each of the index subtypes is scalar.
    //
    // FIXME:  Conditionalize this for debug builds.
    for (unsigned i = 0; i < rank; ++i)
        assert(indices[i]->isScalarType());

    ArrayType *base;
    base = resource.createArrayType(rank, indices, component, isConstrained);

    // Create the first subtype.
    IndexConstraint *constraint = 0;
    if (isConstrained)
        constraint = new IndexConstraint(indices, rank);
    CorrespondingType = resource.createArraySubType(name, base, constraint);
}
