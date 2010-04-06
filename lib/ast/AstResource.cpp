//===-- ast/AstResource.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Type.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast_or_null;
using llvm::cast;
using llvm::isa;

AstResource::AstResource(IdentifierPool &idPool)
    : idPool(idPool)
{
    initializeLanguageDefinedNodes();
}

void AstResource::initializeLanguageDefinedNodes()
{
    initializeBoolean();
    initializeCharacter();
    initializeRootInteger();
    initializeInteger();
    initializeNatural();
    initializePositive();
    initializeString();
    initializeExceptions();

    // Initialize the implicit operations for each declared type.
    theBooleanDecl->generateBooleanDeclarations(*this);
    theRootIntegerDecl->generateImplicitDeclarations(*this);
    theIntegerDecl->generateImplicitDeclarations(*this);
    theCharacterDecl->generateImplicitDeclarations(*this);
}

void AstResource::initializeBoolean()
{
    IdentifierInfo *boolId = getIdentifierInfo("Boolean");
    IdentifierInfo *trueId = getIdentifierInfo("true");
    IdentifierInfo *falseId = getIdentifierInfo("false");

    // Create a vector of Id/Loc pairs for the two elements of this enumeration.
    // Declaration order of the True and False enumeration literals is critical.
    // We need False defined first so that it codegens to 0.
    typedef std::pair<IdentifierInfo*, Location> IdLocPair;
    IdLocPair elems[2] = { IdLocPair(falseId, Location()),
                           IdLocPair(trueId, Location()) };
    theBooleanDecl = createEnumDecl(boolId, Location(), &elems[0], 2, 0);
}

void AstResource::initializeCharacter()
{
    // FIXME:  Eventually there will be a general library API for working with
    // character sets.  For now, we just build the type Character by hand.
    //
    // NOTE: The following is just the first half of the full standard character
    // type.
    static const unsigned numNames = 128;
    const char* names[numNames] = {
        "NUL",   "SOH",   "STX",   "ETX",   "EOT",   "ENQ",   "ACK",   "BEL",
        "BS",    "HT",    "LF",    "VT",    "FF",    "CR",    "SO",    "SI",

        "DLE",   "DC1",   "DC2",   "DC3",   "DC4",   "NAK",   "SYN",   "ETB",
        "CAN",   "EM",    "SUB",   "ESC",   "FS",    "GS",    "RS",    "US",

        "' '",   "'!'",   "'\"'",  "'#'",   "'$'",   "'%'",   "'&'",   "'''",
        "'('",   "')'",   "'*'",   "'+'",   "','",   "'-'",   "'.'",   "'/'",

        "'0'",   "'1'",   "'2'",   "'3'",   "'4'",   "'5'",   "'6'",   "'7'",
        "'8'",   "'9'",   "':'",   "';'",   "'<'",   "'='",   "'>'",   "'?'",

        "'@'",   "'A'",   "'B'",   "'C'",   "'D'",   "'E'",   "'F'",   "'G'",
        "'H'",   "'I'",   "'J'",   "'K'",   "'L'",   "'M'",   "'N'",   "'O'",

        "'P'",   "'Q'",   "'R'",   "'S'",   "'T'",   "'U'",   "'V'",   "'W'",
        "'X'",   "'Y'",   "'Z'",   "'['",   "'\\'",  "']'",   "'^'",   "'_'",

        "'`'",   "'a'",   "'b'",   "'c'",   "'d'",   "'e'",   "'f'",   "'g'",
        "'h'",   "'i'",   "'j'",   "'k'",   "'l'",   "'m'",   "'n'",   "'o'",

        "'p'",   "'q'",   "'r'",   "'s'",   "'t'",   "'u'",   "'v'",   "'w'",
        "'x'",   "'y'",   "'z'",   "'{'",   "'|'",   "'}'",   "'~'",   "DEL" };

    IdentifierInfo *charId = getIdentifierInfo("Character");

    typedef std::pair<IdentifierInfo*, Location> IdLocPair;
    IdLocPair elems[numNames];
    for (unsigned i = 0; i < numNames; ++i) {
        IdentifierInfo *id = getIdentifierInfo(names[i]);
        elems[i] = IdLocPair(id, Location());
    }
    theCharacterDecl = createEnumDecl(charId, Location(), elems, numNames, 0);
    theCharacterDecl->markAsCharacterType();
}

void AstResource::initializeRootInteger()
{
    // Define root_integer as a signed 64 bit type.
    //
    // FIXME:  This is target dependent.
    IdentifierInfo *id = getIdentifierInfo("root_integer");
    llvm::APInt lower = llvm::APInt::getSignedMinValue(64);
    llvm::APInt upper = llvm::APInt::getSignedMaxValue(64);
    IntegerLiteral *lowerExpr = new IntegerLiteral(lower, Location());
    IntegerLiteral *upperExpr = new IntegerLiteral(upper, Location());
    theRootIntegerDecl =
        createIntegerDecl(id, Location(), lowerExpr, upperExpr, 0);
}

void AstResource::initializeInteger()
{
    // Define Integer as a signed 32 bit type.
    IdentifierInfo *integerId = getIdentifierInfo("Integer");
    llvm::APInt lower = llvm::APInt::getSignedMinValue(32);
    llvm::APInt upper = llvm::APInt::getSignedMaxValue(32);
    IntegerLiteral *lowerExpr = new IntegerLiteral(lower, Location());
    IntegerLiteral *upperExpr = new IntegerLiteral(upper, Location());
    theIntegerDecl = createIntegerDecl(integerId, Location(),
                                       lowerExpr, upperExpr, 0);
}

void AstResource::initializeNatural()
{
    // FIXME: This is a temporary hack until we have actual subtype declaration
    // nodes.
    IdentifierInfo *name = getIdentifierInfo("Natural");
    IntegerType *type = theIntegerDecl->getType();
    unsigned width = type->getSize();
    llvm::APInt lowInt(width, 0, false);
    llvm::APInt highInt;
    type->getUpperLimit(highInt);

    // Allocate static expressions for the bounds.
    Expr *low = new IntegerLiteral(lowInt, type, Location());
    Expr *high = new IntegerLiteral(highInt, type, Location());

    theNaturalDecl =
        createIntegerSubtypeDecl(name, Location(), type, low, high, 0);
}

void AstResource::initializePositive()
{
    IdentifierInfo *name = getIdentifierInfo("Positive");
    IntegerType *type = theIntegerDecl->getType();
    unsigned width = type->getSize();
    llvm::APInt lowInt(width, 1, false);
    llvm::APInt highInt;
    type->getUpperLimit(highInt);

    // Allocate static expressions for the bounds.
    Expr *low = new IntegerLiteral(lowInt, type, Location());
    Expr *high = new IntegerLiteral(highInt, type, Location());

    thePositiveDecl =
        createIntegerSubtypeDecl(name, Location(), type, low, high, 0);
}

void AstResource::initializeString()
{
    IdentifierInfo *name = getIdentifierInfo("String");
    DiscreteType *indexTy = getThePositiveType();
    DSTDefinition::DSTTag tag = DSTDefinition::Type_DST;
    DSTDefinition *DST = new DSTDefinition(Location(), indexTy, tag);
    theStringDecl = createArrayDecl(name, Location(), 1, &DST,
                                    getTheCharacterType(), false, 0);
}

void AstResource::initializeExceptions()
{
    IdentifierInfo *PEName = getIdentifierInfo("Program_Error");
    ExceptionDecl::ExceptionKind PEKind = ExceptionDecl::Program_Error;
    theProgramError = new ExceptionDecl(PEKind, PEName, Location(), 0);

    IdentifierInfo *CEName = getIdentifierInfo("Constraint_Error");
    ExceptionDecl::ExceptionKind CEKind = ExceptionDecl::Constraint_Error;
    theConstraintError = new ExceptionDecl(CEKind, CEName, Location(), 0);

    IdentifierInfo *AEName = getIdentifierInfo("Assertion_Error");
    ExceptionDecl::ExceptionKind AEKind = ExceptionDecl::Assertion_Error;
    theAssertionError = new ExceptionDecl(AEKind, AEName, Location(), 0);
}

/// Accessors to the language defined types.  We keep these out of line since we
/// do not want a dependence on Decl.h in AstResource.h.
EnumerationType *AstResource::getTheBooleanType() const
{
    return theBooleanDecl->getType();
}

IntegerType *AstResource::getTheRootIntegerType() const
{
    return theRootIntegerDecl->getBaseSubtype();
}

IntegerType *AstResource::getTheIntegerType() const
{
    return theIntegerDecl->getType();
}

IntegerType *AstResource::getTheNaturalType() const
{
    return theNaturalDecl->getType();
}

IntegerType *AstResource::getThePositiveType() const
{
    return thePositiveDecl->getType();
}

EnumerationType *AstResource::getTheCharacterType() const
{
    return theCharacterDecl->getType();
}

ArrayType *AstResource::getTheStringType() const
{
    return theStringDecl->getType();
}

FunctionType *AstResource::getFunctionType(Type **argTypes, unsigned numArgs,
                                           Type *returnType)
{
    llvm::FoldingSetNodeID ID;
    FunctionType::Profile(ID, argTypes, numArgs, returnType);

    void *pos = 0;
    if (FunctionType *uniqued = functionTypes.FindNodeOrInsertPos(ID, pos))
        return uniqued;

    FunctionType *res = new FunctionType(argTypes, numArgs, returnType);
    functionTypes.InsertNode(res, pos);
    return res;
}

ProcedureType *AstResource::getProcedureType(Type **argTypes, unsigned numArgs)
{
    llvm::FoldingSetNodeID ID;
    ProcedureType::Profile(ID, argTypes, numArgs);

    void *pos = 0;
    if (ProcedureType *uniqued = procedureTypes.FindNodeOrInsertPos(ID, pos))
        return uniqued;

    ProcedureType *res = new ProcedureType(argTypes, numArgs);
    procedureTypes.InsertNode(res, pos);
    return res;
}

DomainType *AstResource::createDomainType(DomainTypeDecl *decl)
{
    DomainType *domTy;

    // Construct the root type.
    domTy = new DomainType(decl);
    types.push_back(domTy);

    // Construct a named first subtype of the root.
    domTy = new DomainType(domTy, domTy->getIdInfo());
    types.push_back(domTy);
    return domTy;
}

DomainType *AstResource::createDomainSubtype(DomainType *root,
                                             IdentifierInfo *name)
{
    return new DomainType(root, name);
}

EnumerationDecl *
AstResource::createEnumDecl(IdentifierInfo *name, Location loc,
                            std::pair<IdentifierInfo*, Location> *elems,
                            unsigned numElems, DeclRegion *parent)
{
    EnumerationDecl *res;
    res = new EnumerationDecl(*this, name, loc, elems, numElems, parent);
    decls.push_back(res);
    return res;
}

EnumerationDecl *
AstResource::createEnumSubtypeDecl(IdentifierInfo *name, Location loc,
                                   EnumerationType *subtype,
                                   Expr *lower, Expr *upper,
                                   DeclRegion *region)
{
    EnumerationDecl *res;
    res = new EnumerationDecl(*this, name, loc, subtype, lower, upper, region);
    decls.push_back(res);
    return res;
}

EnumerationDecl *
AstResource::createEnumSubtypeDecl(IdentifierInfo *name, Location loc,
                                   EnumerationType *subtype,
                                   DeclRegion *region)
{
    EnumerationDecl *res;
    res = new EnumerationDecl(*this, name, loc, subtype, region);
    decls.push_back(res);
    return res;
}

EnumerationType *AstResource::createEnumType(EnumerationDecl *decl)
{
    EnumerationType *res = EnumerationType::create(*this, decl);
    types.push_back(res);
    return res;
}

EnumerationType *AstResource::createEnumSubtype(EnumerationType *base,
                                                EnumerationDecl *decl)
{
    EnumerationType *res = EnumerationType::createSubtype(base, decl);
    types.push_back(res);
    return res;
}

EnumerationType *AstResource::createEnumSubtype(EnumerationType *base,
                                                Expr *low, Expr *high,
                                                EnumerationDecl *decl)
{
    EnumerationType *res;
    res = EnumerationType::createConstrainedSubtype(base, low, high, decl);
    types.push_back(res);
    return res;
}

IntegerDecl *AstResource::createIntegerDecl(IdentifierInfo *name, Location loc,
                                            Expr *lowRange, Expr *highRange,
                                            DeclRegion *parent)
{
    IntegerDecl *res;
    res = new IntegerDecl(*this, name, loc, lowRange, highRange, parent);
    decls.push_back(res);
    return res;
}

IntegerDecl *
AstResource::createIntegerSubtypeDecl(IdentifierInfo *name, Location loc,
                                      IntegerType *subtype,
                                      Expr *lower, Expr *upper,
                                      DeclRegion *parent)
{
    IntegerDecl *res = new IntegerDecl
        (*this, name, loc, subtype, lower, upper, parent);
    decls.push_back(res);
    return res;
}

IntegerDecl *
AstResource::createIntegerSubtypeDecl(IdentifierInfo *name, Location loc,
                                      IntegerType *subtype, DeclRegion *parent)
{
    IntegerDecl *res = new IntegerDecl
        (*this, name, loc, subtype, parent);
    decls.push_back(res);
    return res;
}

IntegerType *AstResource::createIntegerType(IntegerDecl *decl,
                                            const llvm::APInt &low,
                                            const llvm::APInt &high)
{
    IntegerType *res = IntegerType::create(*this, decl, low, high);
    types.push_back(res);
    return res;
}

IntegerType *AstResource::createIntegerSubtype(IntegerType *base,
                                               Expr *low, Expr *high,
                                               IntegerDecl *decl)
{
    IntegerType *res;
    res = IntegerType::createConstrainedSubtype(base, low, high, decl);
    types.push_back(res);
    return res;
}

IntegerType *AstResource::createIntegerSubtype(IntegerType *base,
                                               const llvm::APInt &low,
                                               const llvm::APInt &high,
                                               IntegerDecl *decl)
{
    Expr *lowExpr = new IntegerLiteral(low, base, Location());
    Expr *highExpr = new IntegerLiteral(high, base, Location());
    return createIntegerSubtype(base, lowExpr, highExpr, decl);
}

IntegerType *AstResource::createIntegerSubtype(IntegerType *base,
                                               IntegerDecl *decl)
{
    IntegerType *res = IntegerType::createSubtype(base, decl);
    types.push_back(res);
    return res;
}

DiscreteType *AstResource::createDiscreteSubtype(DiscreteType *base,
                                                 Expr *low, Expr *high,
                                                 TypeDecl *decl)
{
    if (IntegerType *intTy = dyn_cast<IntegerType>(base)) {
        IntegerDecl *subdecl = cast_or_null<IntegerDecl>(decl);
        return createIntegerSubtype(intTy, low, high, subdecl);
    }
    else {
        EnumerationType *enumTy = cast<EnumerationType>(base);
        EnumerationDecl *subdecl = cast_or_null<EnumerationDecl>(decl);
        return createEnumSubtype(enumTy, low, high, subdecl);
    }
}


ArrayDecl *AstResource::createArrayDecl(IdentifierInfo *name, Location loc,
                                        unsigned rank, DSTDefinition **indices,
                                        Type *component, bool isConstrained,
                                        DeclRegion *parent)
{
    ArrayDecl *res = new ArrayDecl(*this, name, loc, rank, indices, component,
                                   isConstrained, parent);
    decls.push_back(res);
    return res;
}

ArrayType *AstResource::createArrayType(ArrayDecl *decl,
                                        unsigned rank, DiscreteType **indices,
                                        Type *component, bool isConstrained)
{
    ArrayType *res;
    res = new ArrayType(decl, rank, indices, component, isConstrained);
    types.push_back(res);
    return res;
}

ArrayType *AstResource::createArraySubtype(IdentifierInfo *name,
                                           ArrayType *base,
                                           DiscreteType **indices)
{
    ArrayType *res = new ArrayType(name, base, indices);
    types.push_back(res);
    return res;
}

ArrayType *AstResource::createArraySubtype(IdentifierInfo *name,
                                           ArrayType *base)
{
    ArrayType *res = new ArrayType(name, base);
    types.push_back(res);
    return res;
}

RecordDecl *AstResource::createRecordDecl(IdentifierInfo *name, Location loc,
                                          DeclRegion *parent)
{
    RecordDecl *res = new RecordDecl(*this, name, loc, parent);
    decls.push_back(res);
    return res;
}

RecordType *AstResource::createRecordType(RecordDecl *decl)
{
    RecordType *res = new RecordType(decl);
    types.push_back(res);
    return res;
}

RecordType *AstResource::createRecordSubtype(IdentifierInfo *name,
                                             RecordType *base)
{
    RecordType *res = new RecordType(base, name);
    types.push_back(res);
    return res;
}

AccessDecl *AstResource::createAccessDecl(IdentifierInfo *name, Location loc,
                                          Type *targetType, DeclRegion *parent)
{
    AccessDecl *result = new AccessDecl(*this, name, loc, targetType, parent);
    decls.push_back(result);
    return result;
}

AccessType *AstResource::createAccessType(AccessDecl *decl, Type *targetType)
{
    AccessType *result = new AccessType(decl, targetType);
    types.push_back(result);
    return result;
}

AccessType *AstResource::createAccessSubtype(IdentifierInfo *name,
                                             AccessType *base)
{
    AccessType *result = new AccessType(base, name);
    types.push_back(result);
    return result;
}

IncompleteTypeDecl *
AstResource::createIncompleteTypeDecl(IdentifierInfo *name, Location loc,
                                      DeclRegion *parent)
{
    IncompleteTypeDecl *res = new IncompleteTypeDecl(*this, name, loc, parent);
    decls.push_back(res);
    return res;
}

IncompleteType *AstResource::createIncompleteType(IncompleteTypeDecl *decl)
{
    IncompleteType *res = new IncompleteType(decl);
    types.push_back(res);
    return res;
}

IncompleteType *AstResource::createIncompleteSubtype(IdentifierInfo *name,
                                                     IncompleteType *base)
{
    IncompleteType *res = new IncompleteType(base, name);
    types.push_back(res);
    return res;
}

ExceptionDecl *AstResource::createExceptionDecl(IdentifierInfo *name,
                                                Location loc,
                                                DeclRegion *region)
{
    ExceptionDecl::ExceptionKind ID = ExceptionDecl::User;
    return new ExceptionDecl(ID, name, loc, region);
}

FunctionDecl *
AstResource::createPrimitiveDecl(PO::PrimitiveID ID, Location loc,
                                 Type *type, DeclRegion *region)
{
    assert(PO::denotesOperator(ID) && "Not a primitive operator!");

    IdentifierInfo *name = getIdentifierInfo(PO::getOpName(ID));
    IdentifierInfo *left = getIdentifierInfo("Left");
    IdentifierInfo *right = getIdentifierInfo("Right");

    // Create the parameter declarations.
    llvm::SmallVector<ParamValueDecl *, 2> params;

    if (ID == PO::POW_op) {
        Type *natural = getTheNaturalType();
        params.push_back(new ParamValueDecl(
                             left, type, PM::MODE_DEFAULT, Location()));
        params.push_back(new ParamValueDecl(
                             left, natural, PM::MODE_DEFAULT, Location()));
    } else if (PO::denotesBinaryOp(ID)) {
        params.push_back(new ParamValueDecl(
                             left, type, PM::MODE_DEFAULT, Location()));
        params.push_back(new ParamValueDecl(
                             right, type, PM::MODE_DEFAULT, Location()));
    }
    else {
        assert(PO::denotesUnaryOp(ID) && "Unexpected operator kind!");
        params.push_back(new ParamValueDecl(
                             right, type, PM::MODE_DEFAULT, Location()));
    }

    // Determine the return type.
    Type *returnTy;
    if (PO::denotesPredicateOp(ID))
        returnTy = theBooleanDecl->getType();
    else
        returnTy = type;

    FunctionDecl *op;
    op = new FunctionDecl(*this, name, loc,
                          &params[0], params.size(), returnTy, region);
    op->setAsPrimitive(ID);
    return op;
}
