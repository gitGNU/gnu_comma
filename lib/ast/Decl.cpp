//===-- ast/Decl.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/AttribDecl.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DeclRewriter.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Stmt.h"

#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <cstring>
#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Decl

DeclRegion *Decl::asDeclRegion() const
{
    const DeclRegion *region = 0;

    switch (getKind()) {
    default:
        break;;
    case AST_PkgInstanceDecl:
        region = static_cast<const PkgInstanceDecl*>(this);
        break;
    case AST_EnumerationDecl:
        region = static_cast<const EnumerationDecl*>(this);
        break;
    case AST_BodyDecl:
        region = static_cast<const BodyDecl*>(this);
        break;
    case AST_FunctionDecl:
        region = static_cast<const FunctionDecl*>(this);
        break;
    case AST_ProcedureDecl:
        region = static_cast<const ProcedureDecl*>(this);
        break;
    case AST_IntegerDecl:
        region = static_cast<const IntegerDecl*>(this);
        break;
    case AST_AccessDecl:
        region = static_cast<const AccessDecl*>(this);
        break;
    case AST_PrivateTypeDecl:
        region = static_cast<const PrivateTypeDecl*>(this);
        break;
    case AST_ArrayDecl:
        region = static_cast<const PrivateTypeDecl*>(this);
        break;
    case AST_RecordDecl:
        region = static_cast<const PrivateTypeDecl*>(this);
        break;
    }
    return const_cast<DeclRegion*>(region);
}

Decl *Decl::resolveOrigin()
{
    Decl *res = this;

    while (res->hasOrigin())
        res = res->getOrigin();

    return res;
}

//===----------------------------------------------------------------------===//
// PackageDecl
PackageDecl::PackageDecl(AstResource &resource, IdentifierInfo *name,
                         Location loc)
        : Decl(AST_PackageDecl, name, loc),
          DeclRegion(AST_PackageDecl),
          resource(resource),
          implementation(0),
          privateDeclarations(0),
          instance(0) { }

PackageDecl::~PackageDecl()
{
    delete implementation;
}

void PackageDecl::setImplementation(BodyDecl *body)
{
    assert(implementation == 0 && "Cannot reset package body!");
    assert(body != 0 && "Cannot set null package body!");

    implementation = body;
}

void PackageDecl::setPrivatePart(PrivatePart *ppart)
{
    assert(privateDeclarations == 0 && "Cannot reset private part!");
    assert(ppart != 0 && "Cannot set null private part!");

    privateDeclarations = ppart;
}

PkgInstanceDecl *PackageDecl::getInstance()
{
    if (instance)
        return instance;

    instance = new PkgInstanceDecl(getIdInfo(), getLocation(), this);
    return instance;
}

//===----------------------------------------------------------------------===//
// PrivatePart

PrivatePart::PrivatePart(PackageDecl *package, Location loc)
  : Ast(AST_PrivatePart),
    DeclRegion(AST_PrivatePart, package), loc(loc)
{
    package->setPrivatePart(this);
}

//===----------------------------------------------------------------------===//
// BodyDecl

BodyDecl::BodyDecl(PackageDecl *package, Location loc)
    : Decl(AST_BodyDecl),
      DeclRegion(AST_BodyDecl),
      loc(loc)
{
    // Check if the given package has a private part.  The declarative region of
    // a body is always under the private part when it exists.
    if (package->hasPrivatePart())
        setParent(package->getPrivatePart());
    else
        setParent(package);

    package->setImplementation(this);
}

PackageDecl *BodyDecl::getPackage()
{
    DeclRegion *parent = getParent();
    if (isa<PackageDecl>(parent))
        return cast<PackageDecl>(parent);
    else
        return cast<PrivatePart>(parent)->getPackage();
}

//===----------------------------------------------------------------------===//
// SubroutineDecl

SubroutineDecl::SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                               ParamValueDecl **params, unsigned numParams,
                               DeclRegion *parent)
    : Decl(kind, name, loc, parent),
      DeclRegion(kind, parent),
      opID(PO::NotPrimitive),
      numParameters(numParams),
      parameters(0),
      body(0),
      declarationLink(0, FORWARD_TAG)
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
      opID(PO::NotPrimitive),
      numParameters(type->getArity()),
      parameters(0),
      body(0),
      declarationLink(0, FORWARD_TAG)
{
    assert(this->denotesSubroutineDecl());

    if (numParameters == 0)
        return;

    parameters = new ParamValueDecl*[numParameters];
    for (unsigned i = 0; i < numParameters; ++i) {
        Type *paramType = type->getArgType(i);
        ParamValueDecl *param = new ParamValueDecl(
            keywords[i], paramType, PM::MODE_DEFAULT, Location());
        parameters[i] = param;
    }
}

SubroutineDecl::SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                               DeclRegion *parent)
    : Decl(kind, name, loc, parent),
      DeclRegion(kind, parent),
      opID(PO::NotPrimitive),
      numParameters(0),
      parameters(0),
      body(0),
      declarationLink(0, FORWARD_TAG)
{
    assert(this->denotesSubroutineDecl());
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
    // Check that we are not reseting the link, and that the given subroutine if
    // of a compatible kind.
    assert(declarationLink.getPointer() == 0 && "Cannot reset base declaration!");
    assert(((isa<FunctionDecl>(this) && isa<FunctionDecl>(routineDecl)) ||
            (isa<ProcedureDecl>(this) && isa<ProcedureDecl>(routineDecl))) &&
           "Defining declarations must be of the same kind as the parent!");

    // Check that the defining declaration does not already have its link set.
    assert(routineDecl->declarationLink.getPointer() == 0);

    declarationLink.setPointer(routineDecl);
    declarationLink.setInt(DEFINITION_TAG);
    routineDecl->declarationLink.setPointer(this);
    routineDecl->declarationLink.setInt(FORWARD_TAG);
}

bool SubroutineDecl::hasBody() const
{
    return body || getDefiningDeclaration();
}

BlockStmt *SubroutineDecl::getBody()
{
    if (body)
        return body;

    if (SubroutineDecl *definition = getDefiningDeclaration())
        return definition->body;

    return 0;
}

const Pragma *SubroutineDecl::locatePragma(pragma::PragmaID ID) const
{
    const_pragma_iterator I = begin_pragmas();
    const_pragma_iterator E = end_pragmas();
    for ( ; I != E; ++I) {
        if (I->getKind() == ID)
            return &*I;
    }
    return 0;
}

const Pragma *SubroutineDecl::findPragma(pragma::PragmaID ID) const
{
    // Check local pragmas.
    if (const Pragma *pragma = locatePragma(ID))
        return pragma;

    // Check forward declarations.
    if (const SubroutineDecl *fwdDecl = getForwardDeclaration())
        return fwdDecl->locatePragma(ID);

    // Check completions.
    if (const SubroutineDecl *defDecl = getDefiningDeclaration())
        return defDecl->locatePragma(ID);

    // Recurse into any declarations from which we were derived.
    if (const SubroutineDecl *origin = getOrigin())
        return origin->findPragma(ID);

    return 0;
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
                           EnumerationType *returnType, DeclRegion *parent)
    : SubroutineDecl(kind, name, loc, parent)
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

bool FunctionDecl::denotesFunctionDecl(const Ast *node)
{
    AstKind kind = node->getKind();
    return (kind == AST_FunctionDecl || kind == AST_EnumLiteral ||
            llvm::isa<FunctionAttribDecl>(node));
}

//===----------------------------------------------------------------------===//
// TypeDecl
void TypeDecl::generateImplicitDeclarations(AstResource &resource)
{
    // Default implementation does nothing.
}

//===----------------------------------------------------------------------===//
// IncompleteTypeDecl

IncompleteTypeDecl::IncompleteTypeDecl(AstResource &resource,
                                       IdentifierInfo *name, Location loc,
                                       DeclRegion *region)
        : TypeDecl(AST_IncompleteTypeDecl, name, loc, region),
          completion(0)
{
    // Create the root and first subtype defined by this incomplete type
    // declaration.
    IncompleteType *rootType = resource.createIncompleteType(this);
    CorrespondingType = resource.createIncompleteSubtype(name, rootType);
}

bool IncompleteTypeDecl::isCompatibleCompletion(const TypeDecl *decl) const
{
    const DeclRegion *thisRegion = this->getDeclRegion();
    const DeclRegion *completion = decl->getDeclRegion();

    if (this->hasCompletion())
        return false;

    if (thisRegion == completion)
        return true;

    if (isa<BodyDecl>(completion) && thisRegion == completion->getParent())
        return true;

    return false;
}

bool IncompleteTypeDecl::completionIsVisibleIn(const DeclRegion *region) const
{
    if (!hasCompletion())
        return false;

    const DeclRegion *target = getDeclRegion();

    do {
        if (target == region)
            return true;
    } while ((region = region->getParent()));

    return false;
}

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
                         EnumerationType *type, EnumerationDecl *parent)
    : FunctionDecl(AST_EnumLiteral, resource, name, loc, type, parent),
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
    : TypeDecl(AST_EnumerationDecl, name, loc, parent),
      DeclRegion(AST_EnumerationDecl, parent),
      numLiterals(numElems)
{
    // Build the root type corresponding to this declaration.
    EnumerationType *root = resource.createEnumType(this);

    // Now, we have a bootstrap issue here to contend with.  We need to perform
    // the following sequence of actions:
    //
    //    - Build the root type of this declaration.
    //
    //    - Build the first constrained subtype of the root type.
    //
    //    - Build the set of EnumLiteral's associated with this declaration,
    //      each of which has the first constrained subtype as type.
    //
    // However, in order to construct the first subtype we need the literals to
    // be available so that we may form the constraint, but we also need the
    // first subtype available to construct the literals.
    //
    // The solution to this circularity is to specify the constraint of the
    // first subtype as attribute expressions over the base subtype.  In source
    // code, the constraint would be similar to "E'Base'First .. E'Base'Last".
    // Note that these attributes are static expressions.
    EnumerationType *base = root->getBaseSubtype();
    Expr *lower = new FirstAE(base, Location());
    Expr *upper = new LastAE(base, Location());

    // Construct the subtype.
    EnumerationType *subtype;
    subtype = resource.createEnumSubtype(root, lower, upper);
    CorrespondingType = subtype;

    // Construct enumeration literals for each Id/Location pair and add them to
    // this decls declarative region.
    for (unsigned i = 0; i < numElems; ++i) {
        IdentifierInfo *name = elems[i].first;
        Location loc = elems[i].second;
        EnumLiteral *elem =
            new EnumLiteral(resource, name, loc, i, subtype, this);
        addDecl(elem);
    }

    // Now that the type is in place initialize the required attribute
    // declarations.
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

EnumerationDecl::EnumerationDecl(AstResource &resource, IdentifierInfo *name,
                                 Location loc,
                                 EnumerationType *subtype, DeclRegion *region)
    : TypeDecl(AST_EnumerationDecl, name, loc, region),
      DeclRegion(AST_EnumerationDecl, region),
      numLiterals(0)
{
    bits |= Subtype_FLAG;       // Mark this as a subtype.
    CorrespondingType = resource.createEnumSubtype(subtype, this);
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

EnumerationDecl::EnumerationDecl(AstResource &resource, IdentifierInfo *name,
                                 Location loc,
                                 EnumerationType *subtype,
                                 Expr *lower, Expr *upper, DeclRegion *region)
    : TypeDecl(AST_EnumerationDecl, name, loc, region),
      DeclRegion(AST_EnumerationDecl, region),
      numLiterals(0)
{
    bits |= Subtype_FLAG;       // Mark this as a subtype.
    CorrespondingType = resource.createEnumSubtype(subtype, lower, upper, this);
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

void EnumerationDecl::generateImplicitDeclarations(AstResource &resource)
{
    // Subtype declarations do not provide any additional operations.
    if (isSubtypeDeclaration())
        return;

    EnumerationType *type = getType();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::EQ_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::NE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::GT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::GE_op, loc, type, this));
}

void EnumerationDecl::generateBooleanDeclarations(AstResource &resource)
{
    // Boolean exports all of the operations a standard enumeration type does,
    // plus the logical operations "and", "or", and "not".
    generateImplicitDeclarations(resource);

    EnumerationType *type = getType();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::LNOT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LAND_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LXOR_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LOR_op, loc, type, this));
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

const EnumLiteral *EnumerationDecl::getFirstLiteral() const
{
    return const_cast<EnumerationDecl*>(this)->getFirstLiteral();
}

EnumLiteral *EnumerationDecl::getFirstLiteral() {
    for (DeclIter I = beginDecls(); I != endDecls(); ++I) {
        if (EnumLiteral *lit = dyn_cast<EnumLiteral>(*I))
            return lit;
    }
    assert(false && "Enumeration decl does not contain any literals!");
    return 0;
}

const EnumLiteral *EnumerationDecl::getLastLiteral() const
{
    return const_cast<EnumerationDecl*>(this)->getLastLiteral();
}

EnumLiteral *EnumerationDecl::getLastLiteral()
{
    for (reverse_decl_iter I = rbegin_decls(); I != rend_decls(); ++I) {
        if (EnumLiteral *lit = dyn_cast<EnumLiteral>(*I))
            return lit;
    }
    assert(false && "Enumeration decl does not contain any literals!");
    return 0;
}

FunctionAttribDecl *EnumerationDecl::getAttribute(attrib::AttributeID ID)
{
    FunctionAttribDecl *attrib = 0;

    switch (ID) {

    default:
        assert(false && "Invalid attribute for enumeration type!");
        attrib = 0;
        break;

    case attrib::Pos:
        attrib = getPosAttribute();
        break;

    case attrib::Val:
        attrib = getValAttribute();
    }

    return attrib;
}

//===----------------------------------------------------------------------===//
// IntegerDecl

IntegerDecl::IntegerDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         Expr *lower, Expr *upper,
                         DeclRegion *parent)
    : TypeDecl(AST_IntegerDecl, name, loc, parent),
      DeclRegion(AST_IntegerDecl, parent),
      lowExpr(lower), highExpr(upper)
{
    llvm::APInt lowVal;
    llvm::APInt highVal;

    assert(lower->isStaticDiscreteExpr());
    assert(upper->isStaticDiscreteExpr());

    lower->staticDiscreteValue(lowVal);
    upper->staticDiscreteValue(highVal);

    IntegerType *base = resource.createIntegerType(this, lowVal, highVal);
    CorrespondingType = resource.createIntegerSubtype(base, lowVal, highVal);

    // Initialize the required attribute declarations now that the type is in
    // place.
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

IntegerDecl::IntegerDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         Expr *modulus, DeclRegion *parent)
    : TypeDecl(AST_IntegerDecl, name, loc, parent),
      DeclRegion(AST_IntegerDecl, parent),
      lowExpr(0), highExpr(modulus)
{
    assert(highExpr->isStaticDiscreteExpr());

    llvm::APInt highVal;
    llvm::APInt lowVal;

    // Extract the contant value of the modulus and form a range constraint from
    // 0 .. modulus - 1.
    modulus->staticDiscreteValue(highVal);

    // Set lowVal to zero.
    lowVal = llvm::APInt::getMinValue(highVal.getBitWidth());

    // FIXME: This should be wrapped in a debug macro.
    if (cast<DiscreteType>(modulus->getType())->isSigned())
        assert(highVal.sgt(lowVal));
    else
        assert(highVal.ugt(lowVal));

    --highVal;

    bits |= Modular_FLAG;

    lowExpr = new IntegerLiteral(lowVal, loc);

    IntegerType *base = resource.createIntegerType(this, lowVal, highVal);
    CorrespondingType = resource.createIntegerSubtype(base, lowVal, highVal);

    // Initialize the required attribute declarations now that the type is in
    // place.
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

IntegerDecl::IntegerDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         IntegerType *subtype, DeclRegion *parent)
    : TypeDecl(AST_IntegerDecl, name, loc, parent),
      DeclRegion(AST_IntegerDecl, parent),
      lowExpr(0), highExpr(0)
{
    // Mark this as a subtype and, if the parent type is modular, as a modular
    // type.
    bits = Subtype_FLAG;
    if (subtype->isModular())
        bits |= Modular_FLAG;

    CorrespondingType = resource.createIntegerSubtype(subtype, this);
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

IntegerDecl::IntegerDecl(AstResource &resource,
                         IdentifierInfo *name, Location loc,
                         IntegerType *subtype,
                         Expr *lower, Expr *upper, DeclRegion *parent)
    : TypeDecl(AST_IntegerDecl, name, loc, parent),
      DeclRegion(AST_IntegerDecl, parent),
      lowExpr(lower), highExpr(upper)
{
    // Mark this as a subtype and, if the parent type is modular, as a modular
    // type.
    bits = Subtype_FLAG;
    if (subtype->isModular())
        bits |= Modular_FLAG;

    CorrespondingType = resource.createIntegerSubtype
        (subtype, lower, upper, this);
    posAttribute = PosAD::create(resource, this);
    valAttribute = ValAD::create(resource, this);
}

Expr *IntegerDecl::getLowBoundExpr()
{
    if (!isModularDeclaration())
        return lowExpr;
    return 0;
}

Expr *IntegerDecl::getHighBoundExpr()
{
    if (!isModularDeclaration())
        return highExpr;
    return 0;
}

Expr *IntegerDecl::getModulusExpr()
{
    if (isModularDeclaration()) {
        // The modulus is always associated with the defining declaration of the
        // root type.
        if (isSubtypeDeclaration()) {
            IntegerType *rootType;
            IntegerDecl *rootDecl;
            rootType = getType()->getRootType();
            rootDecl = cast<IntegerDecl>(rootType->getDefiningDecl());
            return rootDecl->getModulusExpr();
        }
        else
            return highExpr;
    }
    return 0;
}

// Note that we could perform these initializations in the constructor, but it
// would cause difficulties when the language primitive types are first being
// declared.  For now this is a separate method which called separately.
void IntegerDecl::generateImplicitDeclarations(AstResource &resource)
{
    // Subtype declarations do not provide any additional operations.
    if (isSubtypeDeclaration())
        return;

    IntegerType *type = getBaseSubtype();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::EQ_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::NE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::GT_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::LE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::GE_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::ADD_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::SUB_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::MUL_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::DIV_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::MOD_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::REM_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::POW_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::NEG_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::POS_op, loc, type, this));
}

FunctionAttribDecl *IntegerDecl::getAttribute(attrib::AttributeID ID)
{
    FunctionAttribDecl *attrib = 0;

    switch (ID) {

    default:
        assert(false && "Invalid attribute for integer type!");
        attrib = 0;
        break;

    case attrib::Pos:
        attrib = getPosAttribute();
        break;

    case attrib::Val:
        attrib = getValAttribute();
    }

    return attrib;
}

//===----------------------------------------------------------------------===//
// ArrayDecl
ArrayDecl::ArrayDecl(AstResource &resource,
                     IdentifierInfo *name, Location loc,
                     unsigned rank, DSTDefinition **indices,
                     Type *component, bool isConstrained, DeclRegion *parent)
    : TypeDecl(AST_ArrayDecl, name, loc, parent),
      DeclRegion(AST_ArrayDecl, parent),
      indices(indices, indices + rank)
{
    assert(rank != 0 && "Missing indices!");

    // Extract the type nodes of the DSTDefinitions.
    llvm::SmallVector<DiscreteType*, 8> indexTypes(rank);
    for (unsigned i = 0; i < rank; ++i)
        indexTypes[i] = indices[0]->getType();

    ArrayType *base = resource.createArrayType(
        this, rank, &indexTypes[0], component, isConstrained);

    // Create the first subtype.
    CorrespondingType = resource.createArraySubtype(name, base);
}

bool ArrayDecl::isSubtypeDeclaration() const
{
    // FIXME: Extend to support array subtypes.
    return false;
}

//===----------------------------------------------------------------------===//
// RecordDecl
RecordDecl::RecordDecl(AstResource &resource, IdentifierInfo *name,
                       Location loc, DeclRegion *parent)
    : TypeDecl(AST_RecordDecl, name, loc, parent),
      DeclRegion(AST_RecordDecl, parent), componentCount(0)
{
    RecordType *base = resource.createRecordType(this);
    CorrespondingType = resource.createRecordSubtype(name, base);
}

ComponentDecl *RecordDecl::addComponent(IdentifierInfo *name, Location loc,
                                        Type *type)
{
    ComponentDecl *component;
    component = new ComponentDecl(name, loc, type, componentCount, this);
    componentCount++;
    addDecl(component);
    return component;
}

ComponentDecl *RecordDecl::getComponent(unsigned i)
{
    // FIXME: Compensate for the presence of an equality operator when present.
    return cast<ComponentDecl>(getDecl(i));
}

ComponentDecl *RecordDecl::getComponent(IdentifierInfo *name)
{
    PredRange range = findDecls(name);
    if (range.first == range.second)
        return 0;
    return cast<ComponentDecl>(*range.first);
}

bool RecordDecl::isSubtypeDeclaration() const
{
    // FIXME: Extend to support array subtypes.
    return false;
}

//===----------------------------------------------------------------------===//
// AccessDecl
AccessDecl::AccessDecl(AstResource &resource, IdentifierInfo *name,
                       Location loc, Type *targetType, DeclRegion *parent)
    : TypeDecl(AST_AccessDecl, name, loc, parent),
      DeclRegion(AST_AccessDecl, parent)
{
    AccessType *base = resource.createAccessType(this, targetType);
    CorrespondingType = resource.createAccessSubtype(name, base);
}

AccessDecl::AccessDecl(AstResource &resource, IdentifierInfo *name,
                       Location loc, AccessType *baseType, DeclRegion *parent)
    : TypeDecl(AST_AccessDecl, name, loc, parent),
      DeclRegion(AST_AccessDecl, parent)
{
    bits = Subtype_FLAG;
    CorrespondingType = resource.createAccessSubtype(name, baseType);
}

void AccessDecl::generateImplicitDeclarations(AstResource &resource)
{
    // FIXME: We will eventually need to specify the operand type specifically
    // as an unconstrained access type.
    AccessType *type = getType();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::EQ_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::NE_op, loc, type, this));
}

bool AccessDecl::isSubtypeDeclaration() const
{
    return bits & Subtype_FLAG;
}

//===----------------------------------------------------------------------===//
// PrivateTypeDecl
PrivateTypeDecl::PrivateTypeDecl(AstResource &resource,
                                 IdentifierInfo *name, Location loc,
                                 unsigned tags, DeclRegion *context)
    : TypeDecl(AST_PrivateTypeDecl, name, loc, context),
      DeclRegion(AST_PrivateTypeDecl, context),
      completion(0)
{
    CorrespondingType = resource.createPrivateType(this);

    // If the abstract bit was passed ensure the tagged bit is set as well.
    if (tags & Abstract)
        tags = Tagged;

    bits = tags;
}

bool PrivateTypeDecl::isCompatibleCompletion(const TypeDecl *decl) const
{
    const DeclRegion *region = decl->getDeclRegion();
    const PrivatePart *ppart = dyn_cast<PrivatePart>(region);

    // Ensure the decl was declared in the private part of the package defining
    // this private type.
    if (!(ppart && ppart->getParent() == this->getDeclRegion()))
        return false;

    // Ensure the declaration defines a new type.
    return !decl->isSubtypeDeclaration();
}

bool PrivateTypeDecl::completionIsVisibleIn(const DeclRegion *region) const
{
    const DeclRegion *target = getDeclRegion();

    do {
        if (region == target)
            return true;
    } while ((region = region->getParent()));

    return false;
}

void PrivateTypeDecl::generateImplicitDeclarations(AstResource &resource)
{
    if (isLimited())
        return;

    Type *type = getType();
    Location loc = getLocation();

    addDecl(resource.createPrimitiveDecl(PO::EQ_op, loc, type, this));
    addDecl(resource.createPrimitiveDecl(PO::NE_op, loc, type, this));
}

//===----------------------------------------------------------------------===//
// PkgInstanceDecl
PkgInstanceDecl::PkgInstanceDecl(IdentifierInfo *name, Location loc,
                                 PackageDecl *package)
    : Decl(AST_PkgInstanceDecl, name, loc),
      DeclRegion(AST_PkgInstanceDecl, package),
      definition(package)
{
    // Generate our own copy of the public exports provided by the package.
    //
    // FIXME: We should use the DeclRegion observer list for this.
    AstResource &resource = package->getAstResource();
    std::auto_ptr<DeclRewriter> rewriter(
        new DeclRewriter(resource, this, package));
    addDeclarationsUsingRewrites(*rewriter, package);
}
