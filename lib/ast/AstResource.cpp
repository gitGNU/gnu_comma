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
    initializeRootInteger();
    initializeInteger();
    initializeNatural();

    // Initialize the implicit operations for each declared type.
    theBooleanDecl->generateImplicitDeclarations(*this);
    theRootIntegerDecl->generateImplicitDeclarations(*this);
    theIntegerDecl->generateImplicitDeclarations(*this);
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
    theRootIntegerDecl = createIntegerDecl(id, 0,
                                           lowerExpr, upperExpr,
                                           lower, upper, 0);
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
                                       lowerExpr, upperExpr,
                                       lower, upper, 0);
}

void AstResource::initializeNatural()
{
    // FIXME: This is a temporary hack until we have actual subtype declaration
    // nodes.
    IdentifierInfo *name = getIdentifierInfo("Natural");
    IntegerType *type = theIntegerDecl->getType()->getAsIntegerType();
    unsigned width = type->getBitWidth();
    llvm::APInt low(width, 0, false);
    llvm::APInt high(type->getUpperBound());
    theNaturalType = createIntegerSubType(name, type, low, high);
}

/// Accessors to the language defined types.  We keep these out of line since we
/// do not want a dependence on Decl.h in AstResource.h.
EnumSubType *AstResource::getTheBooleanType() const
{
    return theBooleanDecl->getType();
}

IntegerSubType *AstResource::getTheRootIntegerType() const
{
    return theRootIntegerDecl->getBaseSubType();
}

IntegerSubType *AstResource::getTheIntegerType() const
{
    return theIntegerDecl->getType();
}


/// Returns a uniqued FunctionType.
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

/// Returns a uniqued ProcedureType.
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

EnumerationType *AstResource::createEnumType(unsigned numElements)
{
    EnumerationType *res = new EnumerationType(numElements);
    types.push_back(res);
    return res;
}

EnumSubType *AstResource::createEnumSubType(IdentifierInfo *name,
                                            EnumerationType *base)
{
    EnumSubType *res = new EnumSubType(name, base);
    types.push_back(res);
    return res;
}

IntegerDecl *AstResource::createIntegerDecl(IdentifierInfo *name, Location loc,
                                            Expr *lowRange, Expr *highRange,
                                            const llvm::APInt &lowVal,
                                            const llvm::APInt &highVal,
                                            DeclRegion *parent)
{
    IntegerDecl *res = new IntegerDecl(*this, name, loc, lowRange, highRange,
                                       lowVal, highVal, parent);
    decls.push_back(res);
    return res;
}

IntegerType *AstResource::createIntegerType(IntegerDecl *decl,
                                            const llvm::APInt &low,
                                            const llvm::APInt &high)
{
    IntegerType *res = new IntegerType(*this, decl, low, high);
    types.push_back(res);
    return res;
}

IntegerSubType *AstResource::createIntegerSubType(IdentifierInfo *name,
                                                  IntegerType *base,
                                                  const llvm::APInt &low,
                                                  const llvm::APInt &high)
{
    IntegerSubType *res = new IntegerSubType(name, base, low, high);
    types.push_back(res);
    return res;
}

IntegerSubType *AstResource::createIntegerSubType(IdentifierInfo *name,
                                                  IntegerType *base)
{
    IntegerSubType *res = new IntegerSubType(name, base);
    types.push_back(res);
    return res;
}

ArrayDecl *AstResource::createArrayDecl(IdentifierInfo *name, Location loc,
                                        unsigned rank, SubType **indices,
                                        Type *component, bool isConstrained,
                                        DeclRegion *parent)
{
    ArrayDecl *res = new ArrayDecl(*this, name, loc, rank, indices, component,
                                   isConstrained, parent);
    decls.push_back(res);
    return res;
}

ArrayType *AstResource::createArrayType(unsigned rank, SubType **indices,
                                        Type *component, bool isConstrained)
{
    ArrayType *res = new ArrayType(rank, indices, component, isConstrained);
    types.push_back(res);
    return res;
}

ArraySubType *AstResource::createArraySubType(IdentifierInfo *name,
                                              ArrayType *base,
                                              IndexConstraint *constraint)
{
    ArraySubType *res = new ArraySubType(name, base, constraint);
    types.push_back(res);
    return res;
}

IdentifierInfo *AstResource::getTypeIdInfo(Type *type)
{
    // Function and procedure types are never named.
    if (isa<SubroutineType>(type))
        return 0;

    // Subtypes can be either anonymous or named.
    if (SubType *subtype = dyn_cast<SubType>(type))
        return subtype->getIdInfo();

    // DomainTypes are always named.
    if (DomainType *dom = dyn_cast<DomainType>(type))
        return dom->getIdInfo();

    // Otherwise, there should be a declaration node having this type as a base.
    std::vector<Decl*>::iterator I = decls.begin();
    std::vector<Decl*>::iterator E = decls.end();
    for ( ; I != E; ++I) {
        if (TypeDecl *decl = dyn_cast<TypeDecl>(decl)) {
            Type *declTy = decl->getType();
            if (SubType *subtype = dyn_cast<SubType>(declTy))
                declTy = subtype->getTypeOf();
            if (type == declTy)
                return decl->getIdInfo();
        }
    }

    return 0;
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
