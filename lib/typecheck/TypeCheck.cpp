//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "RangeChecker.h"
#include "Scope.h"
#include "Stencil.h"
#include "TypeCheck.h"
#include "comma/ast/AstRewriter.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/ExceptionRef.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DiagPrint.h"
#include "comma/ast/DSTDefinition.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/PackageRef.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/STIndication.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <cstring>

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

TypeCheck::TypeCheck(TextManager &manager, Diagnostic &diag,
                     AstResource &resource, CompilationUnit *cunit)
    : manager(manager),
      diagnostic(diag),
      resource(resource),
      compUnit(cunit)
{
    populateInitialEnvironment();
}

TypeCheck::~TypeCheck() { }

// Called when then type checker is constructed.  Populates the top level scope
// with an initial environment.
void TypeCheck::populateInitialEnvironment()
{
    EnumerationDecl *theBoolDecl = resource.getTheBooleanDecl();
    scope.addDirectDecl(theBoolDecl);
    introduceImplicitDecls(theBoolDecl);

    // We do not add root_integer into scope, since it is an anonymous language
    // defined type.
    IntegerDecl *theRootIntegerDecl = resource.getTheRootIntegerDecl();
    introduceImplicitDecls(theRootIntegerDecl);

    IntegerDecl *theIntegerDecl = resource.getTheIntegerDecl();
    scope.addDirectDecl(theIntegerDecl);
    introduceImplicitDecls(theIntegerDecl);

    // Positive and Natural are subtypes of Integer, and so do not export
    // any additional declarations.
    IntegerDecl *thePositiveDecl = resource.getThePositiveDecl();
    scope.addDirectDecl(thePositiveDecl);
    IntegerDecl *theNaturalDecl = resource.getTheNaturalDecl();
    scope.addDirectDecl(theNaturalDecl);

    EnumerationDecl *theCharacterDecl = resource.getTheCharacterDecl();
    scope.addDirectDecl(theCharacterDecl);
    introduceImplicitDecls(theCharacterDecl);

    ArrayDecl *theStringDecl = resource.getTheStringDecl();
    scope.addDirectDecl(theStringDecl);

    // Add the standard exception objects into scope.
    scope.addDirectDecl(resource.getTheProgramError());
    scope.addDirectDecl(resource.getTheConstraintError());
    scope.addDirectDecl(resource.getTheAssertionError());
}

void TypeCheck::deleteNode(Node &node)
{
    Ast *ast = lift_node<Ast>(node);
    if (ast && ast->isDeletable())
        delete ast;
    node.release();
}


PackageDecl *TypeCheck::getCurrentPackage() const
{
    return currentPackage;
}

SubroutineDecl *TypeCheck::getCurrentSubroutine() const
{
    DeclRegion     *region = currentDeclarativeRegion();
    SubroutineDecl *routine;

    while (region) {
        if ((routine = dyn_cast<SubroutineDecl>(region)))
            return routine;
        region = region->getParent();
    }
    return 0;
}

ProcedureDecl *TypeCheck::getCurrentProcedure() const
{
    return dyn_cast_or_null<ProcedureDecl>(getCurrentSubroutine());
}

FunctionDecl *TypeCheck::getCurrentFunction() const
{
    return dyn_cast_or_null<FunctionDecl>(getCurrentSubroutine());
}

void TypeCheck::acceptWithClause(Location loc, IdentifierInfo **names,
                                 unsigned numNames)
{
    // FIXME: Support qualified names.
    assert(numNames == 1 && "Qualified components not supported yet!");

    // Find the dependency in the current compilation unit.  This operation must
    // succeed.
    typedef CompilationUnit::dep_iterator iterator;
    CompilationUnit *CU = getCompilationUnit();
    Decl *dependency = 0;

    for (iterator I = CU->begin_dependencies();
         I != CU->end_dependencies(); ++I) {
        Decl *candidate = *I;
        if (candidate->getIdInfo() == names[0]) {
            dependency = candidate;
            break;
        }
    }
    assert(dependency && "Failed to resolve withed component!");

    // Bring the dependency into scope.  The only time a conflict can occur is
    // when there are multiple 'with' clauses.
    if (Decl *conflict = scope.addDirectDecl(dependency)) {
        report(loc, diag::MULTIPLE_WITH_CLAUSES)
            << dependency->getIdInfo() << getSourceLoc(conflict->getLocation());
    }
}

TypeDecl *TypeCheck::ensureCompleteTypeDecl(Decl *decl, Location loc,
                                            bool report)
{
    if (TypeDecl *tyDecl = ensureTypeDecl(decl, loc, report)) {
        IncompleteTypeDecl *ITD = dyn_cast<IncompleteTypeDecl>(tyDecl);
        if (ITD) {
            if (ITD->completionIsVisibleIn(currentDeclarativeRegion()))
                return ITD->getCompletion();
            else {
                this->report(loc, diag::INVALID_CONTEXT_FOR_INCOMPLETE_TYPE);
                return 0;
            }
        }
        return tyDecl;
    }
    return 0;
}

TypeDecl *TypeCheck::ensureCompleteTypeDecl(Node node, bool report)
{
    if (TypeRef *ref = lift_node<TypeRef>(node)) {
        return ensureCompleteTypeDecl(ref->getDecl(), ref->getLocation(),
                                      report);
    }
    else if (report) {
        this->report(getNodeLoc(node), diag::NOT_A_TYPE);
    }
    return 0;
}

TypeDecl *TypeCheck::ensureTypeDecl(Decl *decl, Location loc, bool report)
{
    if (TypeDecl *tyDecl = dyn_cast<TypeDecl>(decl))
        return tyDecl;
    if (report)
        this->report(loc, diag::NOT_A_TYPE);
    return 0;
}

TypeDecl *TypeCheck::ensureTypeDecl(Node node, bool report)
{
    if (TypeRef *ref = lift_node<TypeRef>(node)) {
        return ensureTypeDecl(ref->getDecl(), ref->getLocation(), report);
    }
    else if (report) {
        this->report(getNodeLoc(node), diag::NOT_A_TYPE);
    }
    return 0;
}

Type *TypeCheck::resolveType(Type *type) const
{
    // If the given type is incomplete or private determine if it is appropriate
    // to resolve the type to its completion.
    if (IncompleteType *opaqueTy = dyn_cast<IncompleteType>(type)) {
        IncompleteTypeDecl *ITD = opaqueTy->getDefiningDecl();
        if (ITD->completionIsVisibleIn(currentDeclarativeRegion()))
            return resolveType(ITD->getCompletion()->getType());
    }
    else if (PrivateType *privateTy = dyn_cast<PrivateType>(type)) {
        PrivateTypeDecl *PTD = privateTy->getDefiningDecl();
        if (PTD->hasCompletion() &&
            PTD->completionIsVisibleIn(currentDeclarativeRegion()))
            return resolveType(PTD->getCompletion()->getType());
    }

    return type;
}

bool TypeCheck::ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result)
{
    if (isa<IntegerType>(expr->getType()) &&
        expr->staticDiscreteValue(result))
        return true;

    report(expr->getLocation(), diag::NON_STATIC_EXPRESSION);
    return false;
}

bool TypeCheck::ensureStaticIntegerExpr(Expr *expr)
{
    if (isa<IntegerType>(expr->getType()) &&
        expr->isStaticDiscreteExpr())
        return true;

    report(expr->getLocation(), diag::NON_STATIC_EXPRESSION);
    return false;
}

bool TypeCheck::ensurePositiveIntegerExpr(Expr *expr)
{
    llvm::APInt zero;
    llvm::APInt value;
    IntegerType *type;

    if (!ensureStaticIntegerExpr(expr, value))
        return false;

    zero = llvm::APInt::getMinValue(value.getBitWidth());
    type = cast<IntegerType>(expr->getType());

    if (type->isSigned() ? value.sgt(zero) : value.ugt(zero))
        return true;
    else {
        report(expr->getLocation(), diag::EXPECTED_POSITIVE_EXPRESSION);
        return false;
    }
}

ArrayType *TypeCheck::getConstrainedArraySubtype(ArrayType *arrTy, Expr *init)
{
    // FIXME: The following code assumes integer index types exclusively.
    // FIXME: Support multidimensional array types.
    assert(!arrTy->isConstrained() && "Array type already constrained!");
    assert(arrTy->getRank() == 1 && "Multidimensional arrays not supported!");

    if (StringLiteral *strLit = dyn_cast<StringLiteral>(init)) {
        unsigned length = strLit->length();
        DiscreteType *idxTy = cast<DiscreteType>(arrTy->getIndexType(0));

        // FIXME:  Support null string literals by generating a null index type.
        assert(length != 0 && "Null string literals not yet supported!");

        // Obtain the lower and upper limits for the index type and ensure that
        // the given literal is representable within those bounds.
        llvm::APInt lower;
        llvm::APInt upper;

        if (const Range *range = idxTy->getConstraint()) {
            assert(range->isStatic() && "FIXME: Support dynamic indices.");
            lower = range->getStaticLowerBound();
            upper = range->getStaticUpperBound();
        }
        else {
            // Use the representational limits.
            idxTy->getLowerLimit(lower);
            idxTy->getUpperLimit(upper);
        }

        // The following subtraction is always valid provided we treat the
        // result as unsigned.  Note that the value computed here is one less
        // than the actual cardinality -- this is to avoid overflow.
        uint64_t cardinality = (upper - lower).getZExtValue();

        // Reduce the non-zero length by one to fit the "zero based" cardinality
        // value.
        --length;

        if (length > cardinality) {
            report(init->getLocation(), diag::TOO_MANY_ELEMENTS_FOR_TYPE)
                << arrTy->getIdInfo();
            return 0;
        }

        // Adjust the upper bound to the length of the literal.
        upper = length;
        upper += lower;

        // Generate expressions for the bounds.
        //
        // FIXME: Support enumeration types by generating Val attribute
        // expressions.
        IntegerType *intTy = cast<IntegerType>(idxTy);
        Expr *lowerExpr = new IntegerLiteral(lower, intTy, Location());
        Expr *upperExpr = new IntegerLiteral(upper, intTy, Location());
        idxTy = resource.createIntegerSubtype(intTy, lowerExpr, upperExpr);
        return resource.createArraySubtype(0, arrTy, &idxTy);
    }

    ArrayType *exprTy = cast<ArrayType>(init->getType());

    // Check that both root types are identical.
    if (exprTy->getRootType() != arrTy->getRootType()) {
        report(init->getLocation(), diag::INCOMPATIBLE_TYPES);
        return 0;
    }

    // If the expression type is statically constrained, propogate the
    // expression's type.  Otherwise, leave the type as unconstrained.
    if (exprTy->isStaticallyConstrained())
        return exprTy;
    return arrTy;
}

ObjectDecl *TypeCheck::acceptArrayObjectDeclaration(Location loc,
                                                    IdentifierInfo *name,
                                                    ArrayType *arrTy,
                                                    Expr *init)
{
    if (!arrTy->isConstrained() && (init == 0)) {
        report(loc, diag::UNCONSTRAINED_ARRAY_OBJECT_REQUIRES_INIT);
        return 0;
    }

    if (init && !(init = checkExprInContext(init, arrTy)))
        return 0;

    // If the array type is unconstrained, use the resolved type of the
    // initializer.
    if (!arrTy->isConstrained())
        arrTy = cast<ArrayType>(init->getType());

    return new ObjectDecl(name, arrTy, loc, init);
}

bool TypeCheck::acceptObjectDeclaration(Location loc, IdentifierInfo *name,
                                        Node STINode, Node initializerNode)
{
    Expr *init = 0;
    ObjectDecl *decl = 0;
    STIndication *STI = cast_node<STIndication>(STINode);
    Type *STIType = resolveType(STI->getType());

    if (!initializerNode.isNull())
        init = ensureExpr(initializerNode);

    if (ArrayType *arrTy = dyn_cast<ArrayType>(STIType))
        decl = acceptArrayObjectDeclaration(loc, name, arrTy, init);
    else {
        if (init) {
            init = checkExprInContext(init, STIType);
            if (!init)
                return false;
        }
        else if (PrivateType *ptype = dyn_cast<PrivateType>(STIType)) {
            // Limited types must have initializers.
            PrivateTypeDecl *pdecl = ptype->getDefiningDecl();
            if (pdecl->isLimited()) {
                report(loc, diag::LIMITED_OBJECT_REQUIRES_INIT);
                return false;
            }
        }
        decl = new ObjectDecl(name, STIType, loc, init);
    }

    if (decl == 0)
        return false;

    // Do not release STINode as we are now finished with it.
    initializerNode.release();

    if (Decl *conflict = scope.addDirectDecl(decl)) {
        SourceLocation sloc = getSourceLoc(conflict->getLocation());
        report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
        return false;
    }
    currentDeclarativeRegion()->addDecl(decl);
    return true;
}

bool TypeCheck::acceptRenamedObjectDeclaration(Location loc,
                                               IdentifierInfo *name,
                                               Node STINode, Node targetNode)
{
    STIndication *STI = cast_node<STIndication>(STINode);
    Expr *target = ensureExpr(targetNode);

    if (!target || !(target = checkExprInContext(target, STI->getType())))
        return false;

    RenamedObjectDecl *decl;
    targetNode.release();
    decl = new RenamedObjectDecl(name, STI->getType(), loc, target);

    if (Decl *conflict = scope.addDirectDecl(decl)) {
        SourceLocation sloc = getSourceLoc(conflict->getLocation());
        report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
        return false;
    }

    currentDeclarativeRegion()->addDecl(decl);
    return true;
}

bool TypeCheck::acceptUseDeclaration(Node usedNode)
{
    if (PackageRef *ref = lift_node<PackageRef>(usedNode)) {
        PkgInstanceDecl *package = ref->getPackageInstance();
        Location loc = ref->getLocation();

        scope.addImport(package);

        // FIXME: Stitch this use clause into the current context.
        new UseDecl(package, loc);
        return true;
    }

    report(getNodeLoc(usedNode), diag::NOT_A_PACKAGE);
    return false;
}

void TypeCheck::beginEnumeration(IdentifierInfo *name, Location loc)
{
    enumStencil.init(name, loc);
}

void TypeCheck::acceptEnumerationIdentifier(IdentifierInfo *name, Location loc)
{
    acceptEnumerationLiteral(name, loc);
}

void TypeCheck::acceptEnumerationCharacter(IdentifierInfo *name, Location loc)
{
    if (acceptEnumerationLiteral(name, loc))
        enumStencil.markAsCharacterType();
}

bool TypeCheck::acceptEnumerationLiteral(IdentifierInfo *name, Location loc)
{
    // Check that the given element name has yet to appear in the set of
    // elements.  If it exists, mark the stencil as invalid and ignore the
    // element.
    EnumDeclStencil::elem_iterator I = enumStencil.begin_elems();
    EnumDeclStencil::elem_iterator E = enumStencil.end_elems();
    for ( ; I != E; ++I) {
        if (I->first == name) {
            enumStencil.markInvalid();
            report(loc, diag::MULTIPLE_ENUMERATION_LITERALS) << name;
            return false;
        }
    }

    // Check that the element does not conflict with the name of the enumeration
    // decl itself.
    if (name == enumStencil.getIdInfo()) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(enumStencil.getLocation());
        return false;
    }

    enumStencil.addElement(name, loc);
    return true;
}

void TypeCheck::endEnumeration()
{
    IdentifierInfo *name = enumStencil.getIdInfo();
    Location loc = enumStencil.getLocation();
    DeclRegion *region = currentDeclarativeRegion();
    EnumDeclStencil::IdLocPair *elems = enumStencil.getElements().data();
    unsigned numElems = enumStencil.numElements();
    EnumerationDecl *decl;

    ASTStencilReseter reseter(enumStencil);

    // It is possible that the enumeration is empty due to previous errors.  Do
    // not even bother constructing such malformed nodes.
    if (!numElems)
        return;

    decl = resource.createEnumDecl(name, loc, elems, numElems, region);

    // Mark the declaration as a character type if any character literals were
    // used to define it.
    if (enumStencil.isCharacterType())
        decl->markAsCharacterType();

    if (introduceTypeDeclaration(decl)) {
        decl->generateImplicitDeclarations(resource);
        introduceImplicitDecls(decl);
    }
}

void TypeCheck::acceptIntegerTypeDecl(IdentifierInfo *name, Location loc,
                                      Node lowNode, Node highNode)
{
    DeclRegion *region = currentDeclarativeRegion();
    Expr *lower = cast_node<Expr>(lowNode);
    Expr *upper = cast_node<Expr>(highNode);
    RangeChecker rangeCheck(*this);

    if (!rangeCheck.checkDeclarationRange(lower, upper))
        return;

    // Obtain an integer type to represent the base type of this declaration and
    // release the range expressions as they are now owned by this new
    // declaration.
    lowNode.release();
    highNode.release();
    IntegerDecl *decl;
    decl = resource.createIntegerDecl(name, loc, lower, upper, region);

    if (introduceTypeDeclaration(decl)) {
        decl->generateImplicitDeclarations(resource);
        introduceImplicitDecls(decl);
    }
}

void TypeCheck::acceptModularTypeDecl(IdentifierInfo *name, Location loc,
                                      Node modulusNode)
{
    DeclRegion *region = currentDeclarativeRegion();
    Expr *modulus = cast_node<Expr>(modulusNode);

    // Ensure the modulus resolves to some integer type.
    if (!checkExprInContext(modulus, Type::CLASS_Integer))
        return;

    // Ensure the modulus is static and positive.
    if (!ensurePositiveIntegerExpr(modulus))
        return;

    // Obtain an integer type to represent the base type of this declaration.
    modulusNode.release();
    IntegerDecl *decl;
    decl = resource.createIntegerDecl(name, loc, modulus, region);

    if (introduceTypeDeclaration(decl)) {
        decl->generateImplicitDeclarations(resource);
        introduceImplicitDecls(decl);
    }
}

void TypeCheck::acceptRangedSubtypeDecl(IdentifierInfo *name, Location loc,
                                        Node subtypeNode,
                                        Node lowNode, Node highNode)
{
    DeclRegion *region = currentDeclarativeRegion();

    TypeRef *tyRef = lift_node<TypeRef>(subtypeNode);
    if (!tyRef) {
        report(getNodeLoc(subtypeNode), diag::DOES_NOT_DENOTE_A_TYPE);
        return;
    }

    TypeDecl *tyDecl = tyRef->getDecl();
    if (!tyDecl) {
        report(tyRef->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
        return;
    }

    DiscreteType *baseTy = dyn_cast<DiscreteType>(tyDecl->getType());
    if (!baseTy) {
        report(tyRef->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
        return;
    }

    // Convert each of the constraints to the expressions and evaluate them in
    // the context of the subtype indication.
    Expr *lower = ensureExpr(lowNode);
    Expr *upper = ensureExpr(highNode);

    if (!(lower = checkExprInContext(lower, baseTy)) ||
        !(upper = checkExprInContext(upper, baseTy)))
        return;

    // Construct the specific subtype declaration.
    TypeDecl *decl;

    switch (baseTy->getKind()) {
    default:
        assert(false && "Unexpected discrete type!");
        decl = 0;

    case Ast::AST_IntegerType: {
        IntegerType *intTy = cast<IntegerType>(baseTy);
        decl = resource.createIntegerSubtypeDecl(
            name, loc, intTy, lower, upper, region);
        break;
    }

    case Ast::AST_EnumerationType: {
        EnumerationType *enumTy = cast<EnumerationType>(baseTy);
        decl = resource.createEnumSubtypeDecl(
            name, loc, enumTy, lower, upper, region);
        break;
    }
    }

    subtypeNode.release();
    lowNode.release();
    highNode.release();
    introduceTypeDeclaration(decl);
}

void TypeCheck::acceptSubtypeDecl(IdentifierInfo *name, Location loc,
                                  Node subtypeNode)
{
    DeclRegion *region = currentDeclarativeRegion();
    TypeRef *subtype = lift_node<TypeRef>(subtypeNode);

    if (!subtype) {
        report(getNodeLoc(subtypeNode), diag::DOES_NOT_DENOTE_A_TYPE);
        return;
    }

    TypeDecl *tyDecl = subtype->getDecl();

    if (DiscreteType *discTy = dyn_cast<DiscreteType>(tyDecl->getType())) {
        switch (discTy->getKind()) {
        default:
            assert(false && "Unexpected subtype indication!");
            break;

        case Ast::AST_IntegerType: {
            IntegerType *intTy = cast<IntegerType>(discTy);
            tyDecl = resource.createIntegerSubtypeDecl(name, loc, intTy, region);
            break;
        }

        case Ast::AST_EnumerationType : {
            EnumerationType *enumTy = cast<EnumerationType>(discTy);
            tyDecl = resource.createEnumSubtypeDecl(name, loc, enumTy, region);
            break;
        }
        }
    }
    else if (AccessType *ptrTy = dyn_cast<AccessType>(tyDecl->getType()))
        tyDecl = resource.createAccessSubtypeDecl(name, loc, ptrTy, region);
    else {
        report(subtype->getLocation(), diag::INVALID_SUBTYPE_INDICATION);
        return;
    }

    subtypeNode.release();
    introduceTypeDeclaration(tyDecl);
}

void TypeCheck::acceptIncompleteTypeDecl(IdentifierInfo *name, Location loc)
{
    DeclRegion *region = currentDeclarativeRegion();
    IncompleteTypeDecl *ITD;

    ITD = resource.createIncompleteTypeDecl(name, loc, region);
    introduceTypeDeclaration(ITD);
}

void TypeCheck::acceptArrayDecl(IdentifierInfo *name, Location loc,
                                NodeVector indexNodes, Node componentNode)
{
    assert(!indexNodes.empty() && "No type indices for array type decl!");

    // Build a vector of the DSTDefinition's describing the indices of this
    // array declaration.
    typedef NodeCaster<DSTDefinition> Caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, Caster> Mapper;
    typedef llvm::SmallVector<DSTDefinition*, 8> IndexVec;
    IndexVec indices(Mapper(indexNodes.begin(), Caster()),
                     Mapper(indexNodes.end(), Caster()));

    // Unfortunately the parser does not ensure that all index types are either
    // constrained or unconstrained.  Determine the nature of this array
    // declaration by inspecting the first index, then check that every
    // subsequent index follows the rules.
    DSTDefinition::DSTTag tag = indices[0]->getTag();
    bool isConstrained = tag != DSTDefinition::Unconstrained_DST;
    bool allOK = true;
    for (IndexVec::iterator I = indices.begin(); I != indices.end(); ++I) {
        DSTDefinition *index = *I;
        DSTDefinition::DSTTag tag = index->getTag();
        if (tag == DSTDefinition::Unconstrained_DST && isConstrained) {
            report(index->getLocation(),
                   diag::EXPECTED_CONSTRAINED_ARRAY_INDEX);
            allOK = false;
        }
        else if (tag != DSTDefinition::Unconstrained_DST && !isConstrained) {
            report(index->getLocation(),
                   diag::EXPECTED_UNCONSTRAINED_ARRAY_INDEX);
            allOK = false;
        }
    }

    if (!allOK)
        return;

    // Ensure the component node is in fact a type and that it does not denote
    // an incomplete or indefinite type.
    PrimaryType *componentTy;
    if (TypeDecl *componentDecl = ensureCompleteTypeDecl(componentNode)) {
        componentTy = componentDecl->getType();
        if (componentTy->isIndefiniteType()) {
            report(getNodeLoc(componentNode), diag::INDEFINITE_COMPONENT_TYPE);
            return;
        }
    }
    else
        return;

    // Create the declaration node.
    indexNodes.release();
    componentNode.release();
    DeclRegion *region = currentDeclarativeRegion();
    ArrayDecl *array =
        resource.createArrayDecl(name, loc, indices.size(), &indices[0],
                                 componentTy, isConstrained, region);

    if (introduceTypeDeclaration(array))
        introduceImplicitDecls(array);
}

//===----------------------------------------------------------------------===//
// Record type declaration callbacks.

void TypeCheck::beginRecord(IdentifierInfo *name, Location loc)
{
    DeclRegion *region = currentDeclarativeRegion();
    RecordDecl *record = resource.createRecordDecl(name, loc, region);

    scope.push(RECORD_SCOPE);
    pushDeclarativeRegion(record);
}

void TypeCheck::acceptRecordComponent(IdentifierInfo *name, Location loc,
                                      Node typeNode)
{
    assert(scope.getKind() == RECORD_SCOPE);
    RecordDecl *record = cast<RecordDecl>(currentDeclarativeRegion());
    TypeDecl *tyDecl = ensureCompleteTypeDecl(typeNode);

    if (!tyDecl)
        return;

    Type *componentTy = tyDecl->getType();

    if (componentTy->isIndefiniteType()) {
        report(getNodeLoc(typeNode), diag::INDEFINITE_COMPONENT_TYPE);
        return;
    }

    typeNode.release();
    ComponentDecl *component = record->addComponent(name, loc, componentTy);
    if (Decl *conflict = scope.addDirectDecl(component)) {
        SourceLocation sloc = getSourceLoc(conflict->getLocation());
        report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
    }
}

void TypeCheck::endRecord()
{
    assert(scope.getKind() == RECORD_SCOPE);
    scope.pop();

    RecordDecl *record = cast<RecordDecl>(currentDeclarativeRegion());
    popDeclarativeRegion();
    introduceTypeDeclaration(record);
}

void TypeCheck::acceptAccessTypeDecl(IdentifierInfo *name, Location loc,
                                     Node subtypeNode)
{
    TypeDecl *targetDecl = ensureTypeDecl(subtypeNode);

    if (!targetDecl)
        return;

    subtypeNode.release();
    DeclRegion *region = currentDeclarativeRegion();
    AccessDecl *access;
    access = resource.createAccessDecl(name, loc, targetDecl->getType(), region);
    if (introduceTypeDeclaration(access)) {
        access->generateImplicitDeclarations(resource);
        introduceImplicitDecls(access);
    }
}

void TypeCheck::acceptPrivateTypeDecl(IdentifierInfo *name, Location loc,
                                      unsigned typeTag)
{
    DeclRegion *region = currentDeclarativeRegion();
    PackageDecl *package = dyn_cast<PackageDecl>(region);

    // Private types may only be declared in the public part of a package
    // specification.
    if (!package) {
        report(loc, diag::INVALID_CONTEXT_FOR_PRIVATE_TYPE);
        return;
    }

    // Convert the type tags from the ParseClient encoding to the internal AST
    // encoding.
    unsigned tags = 0;
    tags |= (typeTag & AbstractTypeTag) ? PrivateTypeDecl::Abstract : 0;
    tags |= (typeTag & LimitedTypeTag) ? PrivateTypeDecl::Limited : 0;
    tags |= (typeTag & TaggedTypeTag) ? PrivateTypeDecl::Tagged : 0;

    PrivateTypeDecl *decl =
        new PrivateTypeDecl(resource, name, loc, tags, region);

    if (introduceTypeDeclaration(decl)) {
        decl->generateImplicitDeclarations(resource);
        introduceImplicitDecls(decl);
    }
}

//===----------------------------------------------------------------------===//
// DSTDefinition callbacks.

Node TypeCheck::acceptDSTDefinition(Node name, Node lowerNode, Node upperNode)
{
    TypeRef *ref = lift_node<TypeRef>(name);
    DiscreteType *subtype = 0;

    if (ref) {
        TypeDecl *decl = ref->getDecl();
        subtype = dyn_cast<DiscreteType>(decl->getType());
    }

    if (!subtype) {
        report(getNodeLoc(name), diag::EXPECTED_DISCRETE_SUBTYPE_OR_RANGE);
        return getInvalidNode();
    }

    Expr *lower = ensureExpr(lowerNode);
    Expr *upper = ensureExpr(upperNode);
    if (!(lower && upper))
        return getInvalidNode();

    subtype = RangeChecker(*this).checkSubtypeRange(subtype, lower, upper);
    if (!subtype)
        return getInvalidNode();

    DSTDefinition::DSTTag tag = DSTDefinition::Constrained_DST;
    DSTDefinition *result = new DSTDefinition(ref->getLocation(), subtype, tag);

    lowerNode.release();
    upperNode.release();
    return getNode(result);
}

Node TypeCheck::acceptDSTDefinition(Node nameOrAttribute, bool isUnconstrained)
{
    DSTDefinition *result = 0;

    if (TypeRef *ref = lift_node<TypeRef>(nameOrAttribute)) {
        TypeDecl *decl = ref->getDecl();
        if (DiscreteType *type = dyn_cast<DiscreteType>(decl->getType())) {
            DSTDefinition::DSTTag tag = isUnconstrained ?
                DSTDefinition::Unconstrained_DST : DSTDefinition::Type_DST;
            result = new DSTDefinition(ref->getLocation(), type, tag);
            delete ref;
        }
    }
    else if (RangeAttrib *attrib = lift_node<RangeAttrib>(nameOrAttribute)) {
        DSTDefinition::DSTTag tag = DSTDefinition::Attribute_DST;
        result = new DSTDefinition(attrib->getLocation(), attrib, tag);
    }

    if (!result) {
        report(getNodeLoc(nameOrAttribute),
               diag::EXPECTED_DISCRETE_SUBTYPE_OR_RANGE);
        return getInvalidNode();
    }

    nameOrAttribute.release();
    return getNode(result);
}

Node TypeCheck::acceptDSTDefinition(Node lowerNode, Node upperNode)
{
    Expr *lower = ensureExpr(lowerNode);
    Expr *upper = ensureExpr(upperNode);
    RangeChecker rangeCheck(*this);
    DiscreteType *subtype = 0;

    if (!(lower && upper))
        return getInvalidNode();

    if (!(subtype = rangeCheck.checkDSTRange(lower, upper)))
        return getInvalidNode();

    lowerNode.release();
    upperNode.release();
    DSTDefinition::DSTTag tag = DSTDefinition::Range_DST;
    return getNode(new DSTDefinition(lower->getLocation(), subtype, tag));
}

//===----------------------------------------------------------------------===//
// Subtype indication callbacks.

Node TypeCheck::acceptSubtypeIndication(Node prefix)
{
    TypeRef *ref = lift_node<TypeRef>(prefix);
    TypeDecl *decl;

    if (!ref) {
        report(getNodeLoc(prefix), diag::NOT_A_TYPE);
        return getInvalidNode();
    }

    if (!(decl = ensureCompleteTypeDecl(ref->getDecl(), ref->getLocation())))
        return getInvalidNode();

    // Do not release the prefix as we are done with the TypeRef.
    return getNode(new STIndication(ref->getLocation(), decl->getType()));
}

Node TypeCheck::acceptSubtypeIndication(Node prefix,
                                        Node lowerNode, Node upperNode)
{
    TypeRef *ref = lift_node<TypeRef>(prefix);
    DiscreteType *subtype = 0;

    if (ref) {
        TypeDecl *decl = ref->getDecl();
        subtype = dyn_cast<DiscreteType>(decl->getType());
    }

    if (!subtype) {
        report(getNodeLoc(prefix), diag::EXPECTED_DISCRETE_SUBTYPE_OR_RANGE);
        return getInvalidNode();
    }

    Expr *lower = ensureExpr(lowerNode);
    Expr *upper = ensureExpr(upperNode);
    if (!(lower && upper))
        return getInvalidNode();

    subtype = RangeChecker(*this).checkSubtypeRange(subtype, lower, upper);
    if (!subtype)
        return getInvalidNode();

    lowerNode.release();
    upperNode.release();
    return getNode(new STIndication(ref->getLocation(), subtype));
}

Node TypeCheck::acceptSubtypeIndication(Node prefix, NodeVector &arguments)
{
    TypeRef *ref = lift_node<TypeRef>(prefix);

    if (!ref) {
        report(getNodeLoc(prefix), diag::NOT_A_TYPE);
        return getInvalidNode();
    }

    // We must have a type declaration which resolves to an array type.
    //
    // FIXME: Discriminated records need to be supported here.
    //
    // FIXME: According to ARM 3.6.1 the prefix may also be an unconstrained
    // access subtype, provided its designated subtype is an unconstrained array
    // subtype.
    ArrayDecl *decl = dyn_cast_or_null<ArrayDecl>(ref->getDecl());

    if (!decl) {
        report(ref->getLocation(), diag::INVALID_CONSTRAINT_FOR_TYPE)
            << diag::PrintDecl(ref->getDecl());
        return getInvalidNode();
    }

    // Ensure that the number of indices matches that of the array type.
    if (decl->getRank() != arguments.size()) {
        report(ref->getLocation(), diag::WRONG_NUM_SUBSCRIPTS_FOR_ARRAY);
        return getInvalidNode();
    }

    // Ensure that the array type is itself unconstrained.
    if (decl->getType()->isConstrained()) {
        report(ref->getLocation(), diag::CONSTRAINING_CONSTRAINED_ARRAY);
        return getInvalidNode();
    }

    // Each argument must be a DSTDefinition.  For subtype indications the
    // parser cannot guarantee that each argument is of the required form -- we
    // may have expressions, keyword selectors, etc.  Use a node lifter instead
    // of casting.
    typedef NodeLifter<DSTDefinition> Lifter;
    typedef llvm::mapped_iterator<NodeVector::iterator, Lifter> Mapper;
    typedef llvm::SmallVector<DSTDefinition*, 8> IndexVec;
    typedef llvm::SmallVector<DiscreteType*, 8> TypeVec;
    IndexVec indices(Mapper(arguments.begin(), Lifter()),
                     Mapper(arguments.end(), Lifter()));
    TypeVec indexTypes;

    for (unsigned i = 0; i < indices.size(); ++i) {
        DSTDefinition *DST = indices[i];

        // Check that we successfully lifted the index node to a DSTDefinition.
        if (!DST) {
            report(getNodeLoc(arguments[i]), diag::EXPECTED_DISCRETE_INDEX);
            return getInvalidNode();
        }

        // We should never get an unconstrained index from the parser ("range
        // <>") -- assert this fact.
        assert(DST->getTag() != DSTDefinition::Unconstrained_DST &&
               "Expecting only constrained index types for subtype indication!");

        indexTypes.push_back(DST->getType());
    }

    for (unsigned i = 0; i < indexTypes.size(); ++i) {
        DiscreteType *constraintTy = indexTypes[i];
        DiscreteType *targetTy = decl->getIndexType(i);

        // We know the DSTDefinition is well formed.  Ensure that the index type
        // of the array is compatible.
        if (!covers(constraintTy, targetTy)) {
            report(indices[i]->getLocation(), diag::INCOMPATIBLE_TYPES);
            return getInvalidNode();
        }

        // Check if the constraint requires conversions to the index type.
        //
        // We must not modify types once they are created.  Build a new discrete
        // subtype with the constraint bounds wrapped in conversion expressions.
        if (conversionRequired(constraintTy, targetTy)) {
            Expr *lower = constraintTy->getConstraint()->getLowerBound();
            Expr *upper = constraintTy->getConstraint()->getUpperBound();
            lower = convertIfNeeded(lower->clone(), targetTy);
            upper = convertIfNeeded(upper->clone(), targetTy);
            indexTypes[i] = resource.createDiscreteSubtype(
                targetTy, lower, upper);
        }
    }

    // Build the subtype node and wrap it in an STIndication.  Since we do not
    // store the DSTDefinition or the prefix TypeRef, do not release them.
    ArrayType *base = decl->getType();
    ArrayType *type = resource.createArraySubtype(base, &indexTypes[0]);
    STIndication *STI = new STIndication(ref->getLocation(), type);
    return getNode(STI);
}

void TypeCheck::introduceDeclRegion(DeclRegion *region)
{
    typedef DeclRegion::DeclIter iterator;
    for (iterator I = region->beginDecls(); I != region->endDecls(); ++I) {
        Decl *decl = *I;
        if (Decl *conflict = scope.addDirectDecl(decl)) {
            // Conflicts should only happen with private types.
            assert(isa<PrivateTypeDecl>(conflict) &&
                   "Inconsistent private type decl!");
        }

        switch (decl->getKind()) {
        default:
            break;

        case Ast::AST_ArrayDecl:
        case Ast::AST_EnumerationDecl:
        case Ast::AST_IntegerDecl:
        case Ast::AST_AccessDecl:
            introduceImplicitDecls(cast<DeclRegion>(decl));
            break;
        }
    }
}

void TypeCheck::introduceImplicitDecls(DeclRegion *region)
{
    typedef DeclRegion::DeclIter iterator;
    for (iterator I = region->beginDecls(); I != region->endDecls(); ++I) {
        Decl *decl = *I;
        if (Decl *conflict = scope.addDirectDecl(decl)) {
            report(decl->getLocation(), diag::CONFLICTING_DECLARATION)
                << decl->getIdInfo() << getSourceLoc(conflict->getLocation());
        }
    }
}

bool TypeCheck::introduceTypeDeclaration(TypeDecl *decl)
{
    Decl *conflict = scope.addDirectDecl(decl);

    if (conflict) {
        // If the conflict is an IncompleteTypeDecl check that the given
        // declaration can form its completion.
        if (IncompleteTypeDecl *ITD = dyn_cast<IncompleteTypeDecl>(conflict)) {
            if (ITD->isCompatibleCompletion(decl)) {
                ITD->setCompletion(decl);

                // Ensure the completion lives in the same declarative region as
                // its initial declaration.
                assert(ITD->isDeclaredIn(currentDeclarativeRegion()) &&
                       "Inconsistent region for incomplete type completion!");

                // FIXME:  Remove once the above assertion stops firing.
                if (ITD->getDeclRegion() != currentDeclarativeRegion())
                    currentDeclarativeRegion()->addDecl(decl);

                return true;
            }
        }

        // Otherwise we must have a private type declaration.
        if (PrivateTypeDecl *PTD = dyn_cast<PrivateTypeDecl>(conflict)) {
            if (PTD->isCompatibleCompletion(decl)) {
                PTD->setCompletion(decl);
                currentDeclarativeRegion()->addDecl(decl);
                return true;
            }
        }

        report(decl->getLocation(), diag::CONFLICTING_DECLARATION)
            << decl->getIdInfo() << getSourceLoc(conflict->getLocation());
        return false;
    }

    currentDeclarativeRegion()->addDecl(decl);
    return true;
}

Location TypeCheck::getNodeLoc(Node node)
{
    assert(!node.isNull() && "Cannot get locations from null nodes!");
    assert(node.isValid() && "Cannot get locations from invalid nodes!");

    return cast_node<Ast>(node)->getLocation();
}

bool TypeCheck::covers(Type *A, Type *B)
{
    // A type covers itself.
    if (A == B)
        return true;

    if (A->isUniversalTypeOf(B) || B->isUniversalTypeOf(A))
        return true;

    // If either A or B denote private types, resolve their completions if it is
    // possible to do so with respect to the current context.
    if (PrivateType *ptype = dyn_cast<PrivateType>(A)) {
        PrivateTypeDecl *decl = ptype->getDefiningDecl();
        if (decl->completionIsVisibleIn(currentDeclarativeRegion()) &&
            ptype->hasCompletion())
            return covers(ptype->getCompleteType(), B);
    }

    if (PrivateType *ptype = dyn_cast<PrivateType>(B)) {
        PrivateTypeDecl *decl = ptype->getDefiningDecl();
        if (decl->completionIsVisibleIn(currentDeclarativeRegion()) &&
            ptype->hasCompletion())
            return covers(A, ptype->getCompleteType());
    }

    Type *rootTypeA = A;
    Type *rootTypeB = B;

    // If either A or B are primary, resolve their root types.
    if (PrimaryType *primary = dyn_cast<PrimaryType>(A))
        rootTypeA = primary->getRootType();
    if (PrimaryType *primary = dyn_cast<PrimaryType>(B))
        rootTypeB = primary->getRootType();

    return rootTypeA == rootTypeB;
}

bool TypeCheck::conversionRequired(Type *sourceTy, Type *targetTy)
{
    sourceTy = resolveType(sourceTy);
    targetTy = resolveType(targetTy);

    if (sourceTy == targetTy)
        return false;

    // If the source type is universal_integer convert.
    if (sourceTy->isUniversalIntegerType())
        return true;

    PrimaryType *source = dyn_cast<PrimaryType>(sourceTy);
    PrimaryType *target = dyn_cast<PrimaryType>(targetTy);

    // If either of the types are incomplete, attempt to resolve to their
    // completions.
    if (IncompleteType *IT = dyn_cast_or_null<IncompleteType>(source)) {
        if (IT->hasCompletion())
            source = IT->getCompleteType();
    }
    if (IncompleteType *IT = dyn_cast_or_null<IncompleteType>(target)) {
        if (IT->hasCompletion())
            target = IT->getCompleteType();
    }

    if (!(source && target))
        return false;

    // If the source is a subtype of the target a conversion is not required.
    if (source->isSubtypeOf(target))
        return false;

    // If the target is an unconstrained subtype of a common base, a conversion
    // is not needed.
    if ((source->getRootType() == target->getRootType()) &&
        !target->isConstrained())
        return false;

    return true;
}

Expr *TypeCheck::convertIfNeeded(Expr *expr, Type *target)
{
    if (conversionRequired(expr->getType(), target))
        return new ConversionExpr(expr, target, expr->getLocation());
    return expr;
}

Type *TypeCheck::getCoveringDereference(Type *source, Type *target)
{
    while (AccessType *access = dyn_cast<AccessType>(source)) {
        source = resolveType(access->getTargetType());
        if (covers(source, target))
            return source;
    }
    return 0;
}

Type *TypeCheck::getCoveringDereference(Type *source, Type::Classification ID)
{
    while (AccessType *access = dyn_cast<AccessType>(source)) {
        source = resolveType(access->getTargetType());
        if (source->memberOf(ID))
            return source;
    }
    return 0;
}

Expr *TypeCheck::implicitlyDereference(Expr *expr, Type *target)
{
    Type *source = resolveType(expr);
    while (isa<AccessType>(source)) {
        expr = new DereferenceExpr(expr, expr->getLocation(), true);
        source = resolveType(expr);
        if (covers(source, target))
            return expr;
    }
    assert(false && "Implicit dereferencing failed!");
    return expr;
}

Expr *TypeCheck::implicitlyDereference(Expr *expr, Type::Classification ID)
{
    Type *source = resolveType(expr);
    while (isa<AccessType>(source)) {
        expr = new DereferenceExpr(expr, expr->getLocation(), true);
        source = resolveType(expr);
        if (source->memberOf(ID))
            return expr;
    }
    assert(false && "Implicit dereferencing failed!");
    return expr;
}

void TypeCheck::acceptPragmaImport(Location pragmaLoc,
                                   IdentifierInfo *convention,
                                   Location conventionLoc,
                                   IdentifierInfo *entity,
                                   Location entityLoc,
                                   Node externalNameNode)
{
    llvm::StringRef conventionRef(convention->getString());
    PragmaImport::Convention ID = PragmaImport::getConventionID(conventionRef);

    if (ID == PragmaImport::UNKNOWN_CONVENTION) {
        report(conventionLoc, diag::UNKNOWN_CONVENTION) << convention;
        return;
    }

    Resolver &resolver = scope.getResolver();
    if (!resolver.resolve(entity) || !resolver.hasDirectOverloads()) {
        report(entityLoc, diag::NAME_NOT_VISIBLE) << entity;
        return;
    }

    // We do not support importation of overloaded declarations (yet).
    if (resolver.numDirectOverloads() != 1) {
        report(entityLoc, diag::OVERLOADED_IMPORT_NOT_SUPPORTED);
        return;
    }

    // Resolve the external name to a static string expression.
    Expr *name = cast_node<Expr>(externalNameNode);
    if (!(name = checkExprInContext(name, resource.getTheStringType())))
        return;
    if (!name->isStaticStringExpr()) {
        report(name->getLocation(), diag::NON_STATIC_EXPRESSION);
        return;
    }

    // Ensure the declaration does not already have a import pragma associated
    // with it.
    SubroutineDecl *srDecl =
        cast<SubroutineDecl>(resolver.getDirectOverload(0));
    if (srDecl->hasPragma(pragma::Import)) {
        report(entityLoc, diag::DUPLICATE_IMPORT_PRAGMAS) << entity;
        return;
    }

    // The pragma checks out.
    //
    // FIXME: We should also ensure that the declaration profile is conformant
    // with the convention (does not involve unconstrained array types, for
    // example).
    externalNameNode.release();
    PragmaImport *pragma = new PragmaImport(pragmaLoc, ID, entity, name);
    srDecl->attachPragma(pragma);
}

PragmaAssert *TypeCheck::acceptPragmaAssert(Location loc, NodeVector &args)
{
    // Assert pragmas take a required boolean valued predicate and an optional
    // string valued message.  The parser is aware of this.
    Expr *pred = ensureExpr(args[0]);
    Expr *msg = 0;

    if (!(pred && checkExprInContext(pred, resource.getTheBooleanType())))
        return 0;

    if (args.size() == 2) {
        if (!(msg = ensureExpr(args[1])))
            return 0;
        if (!(msg = checkExprInContext(msg, resource.getTheStringType())))
            return 0;
    }

    return new PragmaAssert(loc, pred, msg);
}

Checker *Checker::create(TextManager &manager, Diagnostic &diag,
                         AstResource &resource, CompilationUnit *cunit)
{
    return new TypeCheck(manager, diag, resource, cunit);
}
