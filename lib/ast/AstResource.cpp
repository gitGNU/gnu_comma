//===-- ast/AstResource.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Type.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

AstResource::AstResource(TextProvider &txtProvider, IdentifierPool &idPool)
    : txtProvider(txtProvider),
      idPool(idPool)
{
    initializeLanguageDefinedTypes();
}

void AstResource::initializeLanguageDefinedTypes()
{
    // Build the declaration nodes for each language defined type.
    initializeBoolean();
    initializeCharacter();
    initializeRootInteger();
    initializeInteger();
    initializeNatural();
    initializePositive();
    initializeString();

    // Initialize the implicit operations for each declared type.
    theBooleanDecl->generateImplicitDeclarations(*this);
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
    IdLocPair elems[2] = { IdLocPair(falseId, 0), IdLocPair(trueId, 0) };
    theBooleanDecl = createEnumDecl(boolId, 0, &elems[0], 2, 0);
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
        elems[i] = IdLocPair(id, 0);
    }
    theCharacterDecl = createEnumDecl(charId, 0, elems, numNames, 0);
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
    IntegerLiteral *lowerExpr = new IntegerLiteral(lower, 0);
    IntegerLiteral *upperExpr = new IntegerLiteral(upper, 0);
    theRootIntegerDecl =
        createIntegerDecl(id, 0, lowerExpr, upperExpr, 0);
}

void AstResource::initializeInteger()
{
    // Define Integer as a signed 32 bit type.
    IdentifierInfo *integerId = getIdentifierInfo("Integer");
    llvm::APInt lower = llvm::APInt::getSignedMinValue(32);
    llvm::APInt upper = llvm::APInt::getSignedMaxValue(32);
    IntegerLiteral *lowerExpr = new IntegerLiteral(lower, 0);
    IntegerLiteral *upperExpr = new IntegerLiteral(upper, 0);
    theIntegerDecl = createIntegerDecl(integerId, 0,
                                       lowerExpr, upperExpr, 0);
}

void AstResource::initializeNatural()
{
    // FIXME: This is a temporary hack until we have actual subtype declaration
    // nodes.
    IdentifierInfo *name = getIdentifierInfo("Natural");
    IntegerType *type = theIntegerDecl->getType()->getAsIntegerType();
    unsigned width = type->getSize();
    llvm::APInt low(width, 0, false);
    llvm::APInt high;
    type->getUpperLimit(high);
    theNaturalType = createIntegerSubtype(name, type, low, high);
}

void AstResource::initializePositive()
{
    // FIXME: This is a temporary hack until we have actual subtype declaration
    // nodes.
    IdentifierInfo *name = getIdentifierInfo("Positive");
    IntegerType *type = theIntegerDecl->getType()->getAsIntegerType();
    unsigned width = type->getSize();
    llvm::APInt low(width, 1, false);
    llvm::APInt high;
    type->getUpperLimit(high);
    thePositiveType = createIntegerSubtype(name, type, low, high);
}

void AstResource::initializeString()
{
    IdentifierInfo *name = getIdentifierInfo("String");
    DiscreteType *indexTy = thePositiveType;
    theStringDecl = createArrayDecl(name, 0, 1, &indexTy,
                                    getTheCharacterType(), false, 0);
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

CarrierType *AstResource::createCarrierType(CarrierDecl *decl,
                                            PrimaryType *type)
{
    CarrierType *Ty = new CarrierType(decl, type);
    types.push_back(Ty);
    return Ty;
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

EnumerationType *AstResource::createEnumType(EnumerationDecl *decl)
{
    EnumerationType *res = EnumerationType::create(*this, decl);
    types.push_back(res);
    return res;
}

EnumerationType *AstResource::createEnumSubtype(IdentifierInfo *name,
                                                EnumerationType *base)
{
    EnumerationType *res = EnumerationType::createSubtype(base, name);
    types.push_back(res);
    return res;
}

EnumerationType *AstResource::createEnumSubtype(IdentifierInfo *name,
                                                EnumerationType *base,
                                                Expr *low, Expr *high)
{
    EnumerationType *res;
    res = EnumerationType::createConstrainedSubtype(base, low, high, name);
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

IntegerType *AstResource::createIntegerType(IntegerDecl *decl,
                                            const llvm::APInt &low,
                                            const llvm::APInt &high)
{
    IntegerType *res = IntegerType::create(*this, decl, low, high);
    types.push_back(res);
    return res;
}

IntegerType *AstResource::createIntegerSubtype(IdentifierInfo *name,
                                               IntegerType *base,
                                               Expr *low, Expr *high)
{
    IntegerType *res;
    res = IntegerType::createConstrainedSubtype(base, low, high, name);
    types.push_back(res);
    return res;
}

IntegerType *AstResource::createIntegerSubtype(IdentifierInfo *name,
                                               IntegerType *base,
                                               const llvm::APInt &low,
                                               const llvm::APInt &high)
{
    Expr *lowExpr = new IntegerLiteral(low, base, 0);
    Expr *highExpr = new IntegerLiteral(high, base, 0);
    return createIntegerSubtype(name, base, lowExpr, highExpr);
}

IntegerType *AstResource::createIntegerSubtype(IdentifierInfo *name,
                                               IntegerType *base)
{
    IntegerType *res = IntegerType::createSubtype(base, name);
    types.push_back(res);
    return res;
}

DiscreteType *AstResource::createDiscreteSubtype(IdentifierInfo *name,
                                                 DiscreteType *base,
                                                 Expr *low, Expr *high)
{
    if (IntegerType *intTy = dyn_cast<IntegerType>(base))
        return createIntegerSubtype(name, intTy, low, high);

    EnumerationType *enumTy = cast<EnumerationType>(base);
    return createEnumSubtype(name, enumTy, low, high);
}


ArrayDecl *AstResource::createArrayDecl(IdentifierInfo *name, Location loc,
                                        unsigned rank, DiscreteType **indices,
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
        params.push_back(new ParamValueDecl(left, type, PM::MODE_DEFAULT, 0));
        params.push_back(
            new ParamValueDecl(left, theNaturalType, PM::MODE_DEFAULT, 0));
    } else if (PO::denotesBinaryOp(ID)) {
        params.push_back(new ParamValueDecl(left, type, PM::MODE_DEFAULT, 0));
        params.push_back(new ParamValueDecl(right, type, PM::MODE_DEFAULT, 0));
    }
    else {
        assert(PO::denotesUnaryOp(ID) && "Unexpected operator kind!");
        params.push_back(new ParamValueDecl(right, type, PM::MODE_DEFAULT, 0));
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
