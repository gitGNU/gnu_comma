//===-- typecheck/DeclProducer.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// This file implements the typecheck methods responsible for the production of
// the implicit declarations required by certain type definitions.
//
//===----------------------------------------------------------------------===//

#include "DeclProducer.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Type.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DeclProducer::DeclProducer(AstResource &resource)
    : resource(resource),
      theBoolDecl(0),
      theIntegerDecl(0)
{
    createTheBoolDecl();
    createTheIntegerDecl();
    createTheNaturalDecl();

    createImplicitDecls(theBoolDecl);
    createImplicitDecls(theIntegerDecl);
}

/// Constructor method for producing a raw Bool decl.  This function does not
/// generate the associated implicit functions, however, the literals True and
/// False are produced.
void DeclProducer::createTheBoolDecl()
{
    IdentifierInfo *boolId = resource.getIdentifierInfo("Bool");
    IdentifierInfo *trueId = resource.getIdentifierInfo("true");
    IdentifierInfo *falseId = resource.getIdentifierInfo("false");

    // Declaration order of the True and False enumeration literals is critical.
    // We need False defined first so that it codegens to 0.
    theBoolDecl = new EnumerationDecl(boolId, 0, 0);
    new EnumLiteral(resource, falseId, 0, theBoolDecl);
    new EnumLiteral(resource, trueId, 0, theBoolDecl);
}

/// Constructor method for producing a raw Integer decl.  This function does not
/// generate the associated implicit functions.
void DeclProducer::createTheIntegerDecl()
{
    IdentifierInfo *integerId = resource.getIdentifierInfo("Integer");

    // Define Integer as a signed 32 bit type.
    llvm::APInt lowVal = llvm::APInt::getSignedMinValue(32);
    llvm::APInt highVal = llvm::APInt::getSignedMaxValue(32);
    IntegerLiteral *lowExpr = new IntegerLiteral(lowVal, 0);
    IntegerLiteral *highExpr = new IntegerLiteral(highVal, 0);
    theIntegerDecl =
        new IntegerDecl(resource, integerId, 0,
                        lowExpr, highExpr, lowVal, highVal, 0);
}

void DeclProducer::createTheNaturalDecl()
{
    IdentifierInfo *name = resource.getIdentifierInfo("Natural");
    IntegerType *type = theIntegerDecl->getType()->getAsIntegerType();
    unsigned width = type->getBitWidth();
    llvm::APInt low(width, 0, false);
    llvm::APInt high(type->getUpperBound());
    RangeConstraint *range = new RangeConstraint(low, high);
    theNaturalType = new IntegerSubType(name, type, range);
}

/// Returns the unique enumeration decl representing Bool.
EnumerationDecl *DeclProducer::getBoolDecl() const
{
    return theBoolDecl;
}

/// Returns the unique enumeration type representing Bool.
SubType *DeclProducer::getBoolType() const
{
    return theBoolDecl->getType();
}

/// Returns the unique integer decl representing Integer.
IntegerDecl *DeclProducer::getIntegerDecl() const
{
    return theIntegerDecl;
}

/// Returns the unique type representing Integer.
SubType *DeclProducer::getIntegerType() const
{
    return theIntegerDecl->getType();
}

/// Generates declarations appropriate for the given enumeration, populating \p
/// enumDecl viewed as a DeclRegion with the results.
void DeclProducer::createImplicitDecls(EnumerationDecl *decl)
{
    SubType *type = decl->getType();
    Location loc = decl->getLocation();

    // Construct the builtin equality function.
    FunctionDecl *equals =
        createPrimitiveDecl(PO::EQ_op, loc, type, decl);
    decl->addDecl(equals);
}

/// Generates declarations appropriate for the given integer declaration,
/// populating \p intDecl viewed as a DeclRegion with the results.
void DeclProducer::createImplicitDecls(IntegerDecl *decl)
{
    SubType *type = decl->getType();
    Location loc = decl->getLocation();

    FunctionDecl *eq =
        createPrimitiveDecl(PO::EQ_op, loc, type, decl);
    FunctionDecl *lt =
        createPrimitiveDecl(PO::LT_op, loc, type, decl);
    FunctionDecl *gt =
        createPrimitiveDecl(PO::GT_op, loc, type, decl);
    FunctionDecl *le =
        createPrimitiveDecl(PO::LE_op, loc, type, decl);
    FunctionDecl *ge =
        createPrimitiveDecl(PO::GE_op, loc, type, decl);
    FunctionDecl *add =
        createPrimitiveDecl(PO::ADD_op, loc, type, decl);
    FunctionDecl *sub =
        createPrimitiveDecl(PO::SUB_op, loc, type, decl);
    FunctionDecl *mul =
        createPrimitiveDecl(PO::MUL_op, loc, type, decl);
    FunctionDecl *pow =
        createPrimitiveDecl(PO::POW_op, loc, type, decl);
    FunctionDecl *neg =
        createPrimitiveDecl(PO::NEG_op, loc, type, decl);
    FunctionDecl *pos =
        createPrimitiveDecl(PO::POS_op, loc, type, decl);

    decl->addDecl(eq);
    decl->addDecl(lt);
    decl->addDecl(gt);
    decl->addDecl(le);
    decl->addDecl(ge);
    decl->addDecl(add);
    decl->addDecl(sub);
    decl->addDecl(mul);
    decl->addDecl(pow);
    decl->addDecl(neg);
    decl->addDecl(pos);
}

FunctionDecl *
DeclProducer::createPrimitiveDecl(PO::PrimitiveID ID, Location loc,
                                  Type *type, DeclRegion *region)
{
    assert(PO::denotesOperator(ID) && "Not a primitive operator!");

    IdentifierInfo *name = resource.getIdentifierInfo(PO::getOpName(ID));
    IdentifierInfo *left = resource.getIdentifierInfo("Left");
    IdentifierInfo *right = resource.getIdentifierInfo("Right");

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
        returnTy = theBoolDecl->getType();
    else
        returnTy = type;

    FunctionDecl *op;
    op = new FunctionDecl(resource, name, loc,
                          &params[0], params.size(), returnTy, region);
    op->setAsPrimitive(ID);
    return op;
}
