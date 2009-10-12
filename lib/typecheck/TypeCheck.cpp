//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "Stencil.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Qualifier.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"
#include "comma/typecheck/TypeCheck.h"

#include "llvm/ADT/DenseMap.h"

#include <algorithm>
#include <cstring>

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

TypeCheck::TypeCheck(Diagnostic      &diag,
                     AstResource     &resource,
                     CompilationUnit *cunit)
    : diagnostic(diag),
      resource(resource),
      compUnit(cunit),
      scope(new Scope),
      arrayStencil(new ArrayDeclStencil()),
      enumStencil(new EnumDeclStencil()),
      routineStencil(new SRDeclStencil())
{
    populateInitialEnvironment();
}

TypeCheck::~TypeCheck()
{
    delete scope;
    delete arrayStencil;
    delete enumStencil;
    delete routineStencil;
}

// Called when then type checker is constructed.  Populates the top level scope
// with an initial environment.
void TypeCheck::populateInitialEnvironment()
{
    EnumerationDecl *theBoolDecl = resource.getTheBooleanDecl();
    scope->addDirectDecl(theBoolDecl);
    introduceImplicitDecls(theBoolDecl);

    // We do not add root_integer into scope, since it is an anonymous language
    // defined type.
    IntegerDecl *theRootIntegerDecl = resource.getTheRootIntegerDecl();
    introduceImplicitDecls(theRootIntegerDecl);

    IntegerDecl *theIntegerDecl = resource.getTheIntegerDecl();
    scope->addDirectDecl(theIntegerDecl);
    introduceImplicitDecls(theIntegerDecl);

    EnumerationDecl *theCharacterDecl = resource.getTheCharacterDecl();
    scope->addDirectDecl(theCharacterDecl);
    introduceImplicitDecls(theCharacterDecl);

    ArrayDecl *theStringDecl = resource.getTheStringDecl();
    scope->addDirectDecl(theStringDecl);
}

void TypeCheck::deleteNode(Node &node)
{
    Ast *ast = lift_node<Ast>(node);
    if (ast && ast->isDeletable())
        delete ast;
    node.release();
}

Sigoid *TypeCheck::getCurrentSigoid() const
{
    return dyn_cast<Sigoid>(getCurrentModel());
}

SignatureDecl *TypeCheck::getCurrentSignature() const
{
    return dyn_cast<SignatureDecl>(getCurrentModel());
}

VarietyDecl *TypeCheck::getCurrentVariety() const
{
    return dyn_cast<VarietyDecl>(getCurrentModel());
}

Domoid *TypeCheck::getCurrentDomoid() const
{
    return dyn_cast<Domoid>(getCurrentModel());
}

DomainDecl *TypeCheck::getCurrentDomain() const
{
    return dyn_cast<DomainDecl>(getCurrentModel());
}

FunctorDecl *TypeCheck::getCurrentFunctor() const
{
    return dyn_cast<FunctorDecl>(getCurrentModel());
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

PercentDecl *TypeCheck::getCurrentPercent() const
{
    if (ModelDecl *model = getCurrentModel())
        return model->getPercent();
    return 0;
}

DomainType *TypeCheck::getCurrentPercentType() const
{
    if (ModelDecl *model = getCurrentModel())
        return model->getPercentType();
    return 0;
}

Node TypeCheck::acceptPercent(Location loc)
{
    TypeRef *ref = 0;

    // We are either processing a model or a generic formal domain.
    //
    // When processing a model, return the associated percent decl.  When
    // processing a generic formal domain, return the AbstractDomainDecl.
    if (ModelDecl *model = getCurrentModel())
        ref = new TypeRef(loc, model->getPercent());
    else {
        // FIXME: Use a cleaner interface when available.
        AbstractDomainDecl *decl = cast<AbstractDomainDecl>(declarativeRegion);
        ref = new TypeRef(loc, decl);
    }
    return getNode(ref);
}

// Returns true if the given decl is equivalent to % in the context of the
// current domain.
//
// FIXME: This does not work when processing a formal domain.
bool TypeCheck::denotesDomainPercent(const Decl *decl)
{
    if (checkingDomain()) {
        DomainDecl *domain = getCurrentDomain();
        const DomainDecl *candidate = dyn_cast<DomainDecl>(decl);
        if (candidate && domain)
            return domain == candidate;
    }
    return false;
}

// Returns true if we are currently checking a functor, and if the given functor
// declaration together with the provided arguments would denote an instance
// which is equivalent to % in the current context.  For example, given:
//
//   domain F (X : T) with
//      procedure Foo (A : F(X));
//      ...
//
// Then "F(X)" is equivalent to %.  More generally, a functor F applied to its
// formal arguments in the body of F is equivalent to %.
//
// This function assumes that the number and types of the supplied arguments are
// compatible with the given functor.
bool TypeCheck::denotesFunctorPercent(const FunctorDecl *functor,
                                      DomainTypeDecl **args, unsigned numArgs)
{
    assert(functor->getArity() == numArgs);

    if (checkingFunctor()) {
        FunctorDecl *currentFunctor = getCurrentFunctor();
        if (currentFunctor != functor)
            return false;
        for (unsigned i = 0; i < numArgs; ++i) {
            DomainType *formal = currentFunctor->getFormalType(i);
            if (formal != args[i]->getType())
                return false;
        }
        return true;
    }
    return false;
}

bool TypeCheck::ensureNonRecursiveInstance(
    FunctorDecl *decl, DomainTypeDecl **args, unsigned numArgs, Location loc)
{
    if (!checkingFunctor() || (decl != getCurrentFunctor()))
        return true;
    for (unsigned i = 0; i < numArgs; ++i) {
        // FIXME: DomainTypeDecls should provide the involvesPercent method.
        DomainType *argTy = args[i]->getType();
        if (argTy->involvesPercent()) {
            report(loc, diag::SELF_RECURSIVE_INSTANCE);
            return false;
        }
    }
    return true;
}

/// Resolves the argument type of a Functor or Variety given previous actual
/// arguments.
///
/// For a dependent argument list of the form <tt>(X : T, Y : U(X))</tt>, this
/// function resolves the type of \c U(X) given an actual parameter for \c X.
/// It is assumed that the actual arguments provided are compatable with the
/// given model.
SigInstanceDecl *
TypeCheck::resolveFormalSignature(ModelDecl *parameterizedModel,
                                  Type **arguments, unsigned numArguments)
{
    assert(parameterizedModel->isParameterized());
    assert(numArguments < parameterizedModel->getArity());

    AstRewriter rewriter(resource);

    // For each actual argument, establish a map from the formal parameter to
    // the actual.
    for (unsigned i = 0; i < numArguments; ++i) {
        Type *formal = parameterizedModel->getFormalType(i);
        Type *actual = arguments[i];
        rewriter.addRewrite(formal, actual);
    }

    SigInstanceDecl *target = parameterizedModel->getFormalSignature(numArguments);
    return rewriter.rewrite(target);
}

Decl *TypeCheck::resolveTypeOrModelDecl(IdentifierInfo *name,
                                        Location loc, DeclRegion *region)
{
    Decl *result = 0;

    if (region) {
        DeclRegion::PredRange range = region->findDecls(name);
        // Search the region for a type of the given name.  Type names do not
        // overload so if the type exists, it is unique, and the first match is
        // accepted.
        for (DeclRegion::PredIter iter = range.first;
             iter != range.second; ++iter) {
            Decl *candidate = *iter;
            if ((result = dyn_cast<ModelDecl>(candidate)) or
                (result = dyn_cast<TypeDecl>(candidate)))
                break;
        }
    }
    else {
        Resolver &resolver = scope->getResolver();
        if (resolver.resolve(name)) {
            if (resolver.hasDirectType())
                result = resolver.getDirectType();
            else if (resolver.hasDirectCapsule())
                result = resolver.getDirectCapsule();
            else if (resolver.hasIndirectTypes()) {
                // For the lookup not to be ambiguous, there must only be one
                // indirect type name accessible.
                if (resolver.numIndirectTypes() > 1 ||
                    resolver.hasIndirectOverloads() ||
                    resolver.hasIndirectValues()) {
                    report(loc, diag::NAME_REQUIRES_QUAL) << name;
                    return 0;
                }
                result = resolver.getIndirectType(0);
            }
        }
    }
    if (result == 0)
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
    return result;
}

// Ensures that the given TypeRef is of a sort compatible with the
// parameterization of a variety or functor (e.g. the TypeRef resolves to a
// DomainTypeDecl).  Returns the resolved DomainTypeDecl on sucess.  Otherwise
// diagnostics are posted and null is returned.
DomainTypeDecl *TypeCheck::ensureValidModelParam(TypeRef *ref)
{
    TypeDecl *arg = ref->getTypeDecl();
    DomainTypeDecl *dom = dyn_cast_or_null<DomainTypeDecl>(arg);
    if (!dom) {
        Location loc = ref->getLocation();
        report(loc, diag::INVALID_TYPE_PARAM) << ref->getIdInfo();
    }
    return dom;
}

TypeRef *
TypeCheck::acceptTypeApplication(TypeRef *ref,
                                 SVImpl<TypeRef*>::Type &posArgs,
                                 SVImpl<KeywordSelector*>::Type &keyedArgs)
{
    Location loc = ref->getLocation();
    IdentifierInfo *name = ref->getIdInfo();

    // If the type reference is complete, we cannot apply any arguments.
    if (ref->isComplete()) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return 0;
    }

    ModelDecl *model = ref->getModelDecl();
    unsigned numPositional = posArgs.size();
    unsigned numKeyed = keyedArgs.size();
    unsigned numArgs = numPositional + numKeyed;

    if (model->getArity() != numArgs) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return 0;
    }

    // Check that the model accepts the given keywords.
    if (!checkModelKeywordArgs(model, numPositional, keyedArgs))
        return 0;

    // Build the a sorted vector of arguments, checking that each type reference
    // denotes a domain.  Similarly build a vector of sorted locations for each
    // argument.
    llvm::SmallVector<DomainTypeDecl *, 8> args(numArgs);
    llvm::SmallVector<Location, 8> argLocs(numArgs);

    // Process the positional parameters.
    for (unsigned i = 0; i < numPositional; ++i) {
        DomainTypeDecl *dom = ensureValidModelParam(posArgs[i]);
        if (!dom)
            return 0;
        args[i] = dom;
        argLocs[i] = posArgs[i]->getLocation();
    }

    // Process the keyed parameters, placing them in their sorted positions
    // (checkModelKeywordArgs has already assured us this mapping will not
    // conflict).
    for (unsigned i = 0; i < numKeyed; ++i) {
        KeywordSelector *selector = keyedArgs[i];
        TypeRef *argRef = keyedArgs[i]->getTypeRef();
        DomainTypeDecl *dom = ensureValidModelParam(argRef);

        if (!dom)
            return 0;

        IdentifierInfo *key = selector->getKeyword();
        unsigned index = unsigned(model->getKeywordIndex(key));
        args[index] = dom;
        argLocs[index] = argRef->getLocation();
    }

    // Check that the arguments satisfy the parameterization constraints of the
    // model.
    if (!checkModelArgs(model, args, argLocs))
        return 0;


    // Obtain a memoized type node for this particular argument set.
    TypeRef *instanceRef = 0;
    if (VarietyDecl *V = dyn_cast<VarietyDecl>(model)) {
        SigInstanceDecl *instance = V->getInstance(&args[0], numArgs);
        instanceRef = new TypeRef(loc, instance);
    }
    else {
        FunctorDecl *F = cast<FunctorDecl>(model);

        // Ensure the requested instance is not self recursive.
        if (!ensureNonRecursiveInstance(F, &args[0], numArgs, loc))
            return false;

        // If this particular functor parameterization is equivalent to %, warn
        // and canonicalize to the unique percent node.
        if (denotesFunctorPercent(F, &args[0], numArgs)) {
            report(loc, diag::PERCENT_EQUIVALENT);
            instanceRef = new TypeRef(loc, getCurrentPercent());
        }
        else {
            DomainInstanceDecl *instance = F->getInstance(&args[0], numArgs);
            instanceRef = new TypeRef(loc, instance);
        }
    }
    return instanceRef;
}

bool TypeCheck::checkModelArgs(ModelDecl *model,
                               SVImpl<DomainTypeDecl*>::Type &args,
                               SVImpl<Location>::Type &argLocs)
{
    AstRewriter rewrites(resource);
    unsigned numArgs = args.size();
    for (unsigned i = 0; i < numArgs; ++i) {
        DomainType *argTy = args[i]->getType();
        Location argLoc = argLocs[i];
        AbstractDomainDecl *target = model->getFormalDecl(i);

        // Extend the rewriter mapping the formal argument type to the type of
        // the actual argument.
        rewrites[target->getType()] = argTy;

        // Check the argument in the using the rewriter as context.
        if (!checkSignatureProfile(rewrites, argTy, target, argLoc))
            return false;
    }

    return true;
}

bool TypeCheck::checkModelKeywordArgs(ModelDecl *model, unsigned numPositional,
                                      SVImpl<KeywordSelector*>::Type &keyedArgs)
{
    unsigned numKeys = keyedArgs.size();
    for (unsigned i = 0; i < numKeys; ++i) {
        KeywordSelector *selector = keyedArgs[i];
        IdentifierInfo *keyword = selector->getKeyword();
        Location keywordLoc = selector->getLocation();
        int keywordIdx = model->getKeywordIndex(keyword);

        // Ensure the given keyword exists.
        if (keywordIdx < 0) {
            report(keywordLoc, diag::TYPE_HAS_NO_SUCH_KEYWORD)
                << keyword << model->getIdInfo();
            return false;
        }

        // The corresponding index of the keyword must be greater than the
        // number of supplied positional parameters (otherwise it would
        // `overlap' a positional parameter).
        if ((unsigned)keywordIdx < numPositional) {
            report(keywordLoc, diag::PARAM_PROVIDED_POSITIONALLY) << keyword;
            return false;
        }

        // Ensure that this keyword is not a duplicate of any preceeding
        // keyword.
        for (unsigned j = 0; j < i; ++j) {
            if (keyedArgs[j]->getKeyword() == keyword) {
                report(keywordLoc, diag::DUPLICATE_KEYWORD) << keyword;
                return false;
            }
        }
    }
    return true;
}

TypeDecl *TypeCheck::ensureTypeDecl(Decl *decl, Location loc, bool report)
{
    if (TypeDecl *tyDecl = dyn_cast<TypeDecl>(decl))
        return tyDecl;
    if (report)
        this->report(loc, diag::TYPE_CANNOT_DENOTE_VALUE);
    return 0;
}

TypeDecl *TypeCheck::ensureTypeDecl(Node node, bool report)
{
    TypeRef *ref = cast_node<TypeRef>(node);
    return ensureTypeDecl(ref->getDecl(), ref->getLocation(), report);
}

bool TypeCheck::ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result)
{
    if (expr->staticIntegerValue(result))
        return true;

    report(expr->getLocation(), diag::NON_STATIC_EXPRESSION);
    return false;
}

bool TypeCheck::ensureStaticIntegerExpr(Expr *expr)
{
    if (expr->isStaticIntegerExpr())
        return true;

    report(expr->getLocation(), diag::NON_STATIC_EXPRESSION);
    return false;
}

ArraySubType *TypeCheck::getConstrainedArraySubType(ArraySubType *arrTy,
                                                    Expr *init)
{
    // FIXME: The following code assumes integer index types exclusively.
    // FIXME: Support multidimensional array types.
    assert(!arrTy->isConstrained() && "Array type already constrained!");
    assert(arrTy->getRank() == 1 && "Multidimensional arrays not supported!");

    if (StringLiteral *strLit = dyn_cast<StringLiteral>(init)) {
        IntegerSubType *idxTy = cast<IntegerSubType>(arrTy->getIndexType(0));

        // FIXME: For empty string literals we should generate a null range
        // which is compatable with the base index type.
        llvm::APInt lower(idxTy->getLowerBound());
        llvm::APInt upper(lower + strLit->length() - 1);

        // FIXME: Check that the index type is large enough to support the
        // length of the literal.
        SubType *newIdxTy;
        newIdxTy = resource.createIntegerSubType(0, idxTy->getTypeOf(),
                                                 lower, upper);
        return resource.createArraySubType(0, arrTy->getTypeOf(),
                                           new IndexConstraint(&newIdxTy, 1));
    }

    ArraySubType *exprTy = cast<ArraySubType>(init->getType());

    if (!exprTy->isConstrained()) {
        // If the expression is unconstrained, we need to create a dynamicly
        // constrained subtype.  These situations arise when the initializer is
        // a function call returning an unconstrained array type, for example.
        //
        // The subtype is constructed using a range for each index of the type
        // corresponding to X'First .. X'Last, where X is the given expression.

        // FIXME: We should be creating a range attribute here.  This is
        // important since a range attribute will not evaluate the expression
        // twice when computing the bounds.
        FirstArrayAE *first = new FirstArrayAE(init, 0);
        LastArrayAE *last = new LastArrayAE(init, 0);

        IntegerSubType *idxTy = cast<IntegerSubType>(arrTy->getIndexType(0));
        SubType *newIdxTy = resource.createIntegerSubType(0, idxTy->getTypeOf(),
                                                          first, last);
        IndexConstraint *constraint = new IndexConstraint(&newIdxTy, 1);
        return resource.createArraySubType(0, arrTy->getTypeOf(), constraint);
    }

    // The initializer is constrained.  Use the type of the initializer.
    return exprTy;
}

ObjectDecl *TypeCheck::acceptArrayObjectDeclaration(Location loc,
                                                    IdentifierInfo *name,
                                                    ArrayDecl *arrDecl,
                                                    Expr *init)
{
    ArraySubType *arrTy = arrDecl->getType();

    if (!arrTy->isConstrained()) {
        // Unconstrained arrays require initialization.
        if (init == 0) {
            report(loc, diag::UNCONSTRAINED_ARRAY_OBJECT_REQUIRES_INIT);
            return 0;
        }

        // Generate a new subtype for the object which matches the initializer.
        if (!(arrTy = getConstrainedArraySubType(arrTy, init)))
            return 0;
    }

    if (init && !checkExprInContext(init, arrTy))
        return 0;

    return new ObjectDecl(name, arrTy, loc, init);
}

bool TypeCheck::acceptObjectDeclaration(Location loc, IdentifierInfo *name,
                                        Node refNode, Node initializerNode)
{
    Expr *init = 0;
    ObjectDecl *decl = 0;
    TypeDecl *tyDecl = ensureTypeDecl(refNode);

    if (!tyDecl) return false;

    if (!initializerNode.isNull())
        init = cast_node<Expr>(initializerNode);

    if (ArrayDecl *arrDecl = dyn_cast<ArrayDecl>(tyDecl)) {
        decl = acceptArrayObjectDeclaration(loc, name, arrDecl, init);
        if (decl == 0)
            return false;
    }
    else {
        Type *objTy = tyDecl->getType();

        if (init) {
            if (!checkExprInContext(init, objTy))
                return false;

            if (conversionRequired(init->getType(), objTy))
                init = new ConversionExpr(init, objTy);
        }
        decl = new ObjectDecl(name, objTy, loc, init);
    }


    initializerNode.release();

    if (Decl *conflict = scope->addDirectDecl(decl)) {
        SourceLocation sloc = getSourceLoc(conflict->getLocation());
        report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
        return false;
    }
    currentDeclarativeRegion()->addDecl(decl);
    return true;
}

bool TypeCheck::acceptImportDeclaration(Node importedNode)
{
    TypeRef *ref = cast_node<TypeRef>(importedNode);
    Decl *decl = ref->getDecl();
    Location loc = ref->getLocation();
    DomainType *domain;

    if (CarrierDecl *carrier = dyn_cast<CarrierDecl>(decl))
        domain = dyn_cast<DomainType>(carrier->getRepresentationType());
    else
        domain = dyn_cast<DomainTypeDecl>(decl)->getType();

    if (!domain) {
        report(loc, diag::IMPORT_FROM_NON_DOMAIN);
        return false;
    }

    scope->addImport(domain);

    // FIXME:  We need to stitch this import declaration into the current
    // context.
    new ImportDecl(domain, loc);
    return true;
}

void TypeCheck::beginEnumeration(IdentifierInfo *name, Location loc)
{
    enumStencil->init(name, loc);
}

void TypeCheck::acceptEnumerationIdentifier(IdentifierInfo *name, Location loc)
{
    acceptEnumerationLiteral(name, loc);
}

void TypeCheck::acceptEnumerationCharacter(IdentifierInfo *name, Location loc)
{
    if (acceptEnumerationLiteral(name, loc))
        enumStencil->markAsCharacterType();
}

bool TypeCheck::acceptEnumerationLiteral(IdentifierInfo *name, Location loc)
{
    // Check that the given element name has yet to appear in the set of
    // elements.  If it exists, mark the stencil as invalid and ignore the
    // element.
    EnumDeclStencil::elem_iterator I = enumStencil->begin_elems();
    EnumDeclStencil::elem_iterator E = enumStencil->end_elems();
    for ( ; I != E; ++I) {
        if (I->first == name) {
            enumStencil->markInvalid();
            report(loc, diag::MULTIPLE_ENUMERATION_LITERALS) << name;
            return false;
        }
    }

    // Check that the element does not conflict with the name of the enumeration
    // decl itself.
    if (name == enumStencil->getIdInfo()) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(enumStencil->getLocation());
        return false;
    }

    enumStencil->addElement(name, loc);
    return true;
}

void TypeCheck::endEnumeration()
{
    IdentifierInfo *name = enumStencil->getIdInfo();
    Location loc = enumStencil->getLocation();
    DeclRegion *region = currentDeclarativeRegion();
    EnumDeclStencil::IdLocPair *elems = enumStencil->getElements().data();
    unsigned numElems = enumStencil->numElements();
    EnumerationDecl *decl;

    ASTStencilReseter reseter(enumStencil);

    // It is possible that the enumeration is empty due to previous errors.  Do
    // not even bother constructing such malformed nodes.
    if (!numElems)
        return;

    decl = resource.createEnumDecl(name, loc, elems, numElems, region);

    // Check that the enumeration does not conflict with any other in the
    // current scope.
    if (Decl *conflict = scope->addDirectDecl(decl)) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return;
    }

    // Mark the declaration as a character type if any character literals were
    // used to define it.
    if (enumStencil->isCharacterType())
        decl->markAsCharacterType();

    region->addDecl(decl);
    decl->generateImplicitDeclarations(resource);
    introduceImplicitDecls(decl);
}

/// Called to process integer type definitions.
///
/// For example, given a definition of the form <tt>type T is range X..Y;</tt>,
/// this callback is invoked with \p name set to the identifier \c T, \p loc set
/// to the location of \p name, \p low set to the expression \c X, and \p high
/// set to the expression \c Y.
void TypeCheck::acceptIntegerTypedef(IdentifierInfo *name, Location loc,
                                     Node lowNode, Node highNode)
{
    DeclRegion *region = currentDeclarativeRegion();
    Expr *lowExpr = cast_node<Expr>(lowNode);
    Expr *highExpr = cast_node<Expr>(highNode);

    // If the bounds are function calls it is possible that they have not been
    // fully resolved since no target context has been seen for these
    // expressions.  We demand that the expressions be of any integer type.
    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(lowExpr)) {
        if (!resolveFunctionCall(call, Type::CLASS_Integer))
            return;
    }
    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(highExpr)) {
        if (!resolveFunctionCall(call, Type::CLASS_Integer))
            return;
    }

    if (!ensureStaticIntegerExpr(lowExpr) || !ensureStaticIntegerExpr(highExpr))
        return;

    // Obtain an integer type to represent the base type of this declaration and
    // release the range expressions as they are now owned by this new
    // declaration.
    lowNode.release();
    highNode.release();
    IntegerDecl *decl;
    decl = resource.createIntegerDecl(name, loc, lowExpr, highExpr, region);

    if (Decl *conflict = scope->addDirectDecl(decl)) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return;
    }

    region->addDecl(decl);
    decl->generateImplicitDeclarations(resource);
    introduceImplicitDecls(decl);
}

//===----------------------------------------------------------------------===//
// Array type definition callbacks.

void TypeCheck::beginArray(IdentifierInfo *name, Location loc)
{
    arrayStencil->init(name, loc);
}

void TypeCheck::acceptUnconstrainedArrayIndex(Node indexNode)
{
    // The parser guarantees that all index definitions will be unconstrained or
    // constrained.  Assert this fact for ourselves.
    if (arrayStencil->numIndices())
        assert(!arrayStencil->isConstrained() &&
               "Conflicting array index definitions!");

    TypeRef *index = lift_node<TypeRef>(indexNode);
    if (!index) {
        arrayStencil->markInvalid();
        report(getNodeLoc(indexNode), diag::EXPECTED_DISCRETE_INDEX);
        return;
    }

    indexNode.release();
    arrayStencil->addIndex(index);
}

void TypeCheck::acceptArrayIndex(Node indexNode)
{
    // The parser guarantees that all index definitions will be unconstrained or
    // constrained.  Assert this fact for ourselves by ensuring that if the
    // current array stencil is not constrained.
    assert(!arrayStencil->isConstrained() &&
           "Conflicting array index definitions!");

    TypeRef *index = lift_node<TypeRef>(indexNode);
    if (!index) {
        arrayStencil->markInvalid();
        report(getNodeLoc(indexNode), diag::EXPECTED_DISCRETE_INDEX);
        return;
    }

    indexNode.release();
    arrayStencil->markAsConstrained();
    arrayStencil->addIndex(index);
}

void TypeCheck::acceptArrayComponent(Node componentNode)
{
    assert(arrayStencil->getComponentType() == 0 &&
           "Array component type already initialized!");

    TypeDecl *componentTy = ensureTypeDecl(componentNode);

    if (!componentTy) {
        arrayStencil->markInvalid();
        return;
    }
    arrayStencil->setComponentType(componentTy);
}

void TypeCheck::endArray()
{
    ASTStencilReseter reseter(arrayStencil);

    // If the array stencil is invalid, do not construct the declaration.
    if (arrayStencil->isInvalid())
        return;

    // Ensure that at least one index has been associated with this stencil.  It
    // is possible that the parser could not parse the index components.  Just
    // return in this case, since the parser would have already posted a
    // diagnostic.
    if (arrayStencil->numIndices() == 0)
        return;

    // Likewise, it is possible that the parser could not complete the component
    // type declaration.
    if (arrayStencil->getComponentType() == 0)
        return;

    IdentifierInfo *name = arrayStencil->getIdInfo();
    Location loc = arrayStencil->getLocation();

    // Ensure that each index type is a discrete type.  Build a vector of all
    // valid SubTypes for each index.
    llvm::SmallVector<SubType*, 4> indices;
    typedef ArrayDeclStencil::index_iterator index_iterator;
    for (index_iterator I = arrayStencil->begin_indices();
         I != arrayStencil->end_indices(); ++I) {
        TypeRef *ref = *I;
        TypeDecl *tyDecl = dyn_cast_or_null<TypeDecl>(ref->getTypeDecl());

        if (!tyDecl || !tyDecl->getType()->isDiscreteType()) {
            report(ref->getLocation(), diag::EXPECTED_DISCRETE_INDEX);
            break;
        }

        // A type declaration of a scalar type always provides the first subtype
        // as its type.
        SubType *subtype = cast<SubType>(tyDecl->getType());
        indices.push_back(subtype);
    }

    // Do not create the array declaration unless all indices checked out.
    if (indices.size() != arrayStencil->numIndices())
        return;

    Type *component = arrayStencil->getComponentType()->getType();
    bool isConstrained = arrayStencil->isConstrained();
    DeclRegion *region = currentDeclarativeRegion();
    ArrayDecl *array;
    array = resource.createArrayDecl(name, loc,
                                     indices.size(), &indices[0],
                                     component, isConstrained, region);

    // Check for conflicts.
    if (Decl *conflict = scope->addDirectDecl(array)) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return;
    }

    // FIXME: We need to introduce the implicit operations for this type.
    region->addDecl(array);
    introduceImplicitDecls(array);
}

bool TypeCheck::checkType(Type *source, SigInstanceDecl *target, Location loc)
{
    if (DomainType *domain = dyn_cast<DomainType>(source)) {
        if (!has(domain, target)) {
            report(loc, diag::DOES_NOT_SATISFY) << target->getString();
            return false;
        }
        return true;
    }

    if (SubType *subtype = dyn_cast<SubType>(source)) {
        DomainType *rep = dyn_cast<DomainType>(subtype->getTypeOf());
        if (!rep) {
            report(loc, diag::NOT_A_DOMAIN);
            return false;
        }
        if (!has(rep, target)) {
            report(loc, diag::DOES_NOT_SATISFY) << target->getString();
            return false;
        }
        return true;
    }

    // Otherwise, the source does not denote a domain, and so cannot satisfy the
    // signature constraint.
    report(loc, diag::NOT_A_DOMAIN);
    return false;
}

bool TypeCheck::checkSignatureProfile(const AstRewriter &rewrites,
                                      Type *source, AbstractDomainDecl *target,
                                      Location loc)
{
    if (DomainType *domain = dyn_cast<DomainType>(source)) {
        if (!has(rewrites, domain, target)) {
            report(loc, diag::DOES_NOT_SATISFY) << target->getString();
            return false;
        }
        return true;
    }

    if (SubType *subtype = dyn_cast<SubType>(source)) {
        Type *rep = dyn_cast<DomainType>(subtype->getTypeOf());
        return checkSignatureProfile(rewrites, rep, target, loc);
    }

    // Otherwise, the source does not denote a domain, and so cannot satisfy the
    // signature constraint.
    report(loc, diag::NOT_A_DOMAIN);
    return false;
}

void TypeCheck::introduceImplicitDecls(DeclRegion *region)
{
    typedef DeclRegion::DeclIter iterator;
    for (iterator I = region->beginDecls(); I != region->endDecls(); ++I) {
        Decl *decl = *I;
        if (Decl *conflict = scope->addDirectDecl(decl)) {
            report(decl->getLocation(), diag::CONFLICTING_DECLARATION)
                << decl->getIdInfo() << getSourceLoc(conflict->getLocation());
        }
    }
}

/// Returns true if the IdentifierInfo \p info can name a binary function.
bool TypeCheck::namesBinaryFunction(IdentifierInfo *info)
{
    const char* name = info->getString();
    size_t length = std::strlen(name);

    if (length > 2)
        return false;

    if (length == 1) {
        switch (*name) {
        default:
            return false;
        case '=':
        case '+':
        case '*':
        case '-':
        case '>':
        case '<':
            return true;
        }
    }
    else
        return (std::strncmp(name, "<=", 2) == 0 ||
                std::strncmp(name, ">=", 2) == 0);
}

bool TypeCheck::namesUnaryFunction(IdentifierInfo *info)
{
    const char* name = info->getString();
    size_t length = std::strlen(name);

    if (length == 1) {
        switch (*name) {
        default:
            return false;
        case '+':
        case '-':
            return true;
        }
    }
    else
        return false;
}

Location TypeCheck::getNodeLoc(Node node)
{
    assert(!node.isNull() && "Cannot get locations from null nodes!");
    assert(node.isValid() && "Cannot get locations from invalid nodes!");

    if (Expr *expr = lift_node<Expr>(node))
        return expr->getLocation();

    if (TypeRef *ref = lift_node<TypeRef>(node))
        return ref->getLocation();

    if (SubroutineRef *ref = lift_node<SubroutineRef>(node))
        return ref->getLocation();

    ProcedureCallStmt *call = cast_node<ProcedureCallStmt>(node);
    return call->getLocation();
}

bool TypeCheck::covers(Type *A, Type *B)
{
    if (subsumes(A, B))
        return true;

    // If B denotes the primitive type root_integer and A is any integer type,
    // then A covers B.
    if (dyn_cast<IntegerSubType>(B) == resource.getTheRootIntegerType())
        return A->isIntegerType();

    return false;
}

bool TypeCheck::subsumes(Type *A, Type *B)
{
    // A type subsumes itself.
    if (A == B)
        return true;

    // If A denotes the primitive type root_integer and B is any integer type,
    // then A subsumes B.
    if (dyn_cast<IntegerSubType>(A) == resource.getTheRootIntegerType())
        if (B->isIntegerType())
            return true;

    Type *baseTypeA = A;
    Type *baseTypeB = B;

    // If either A or B are subtypes, resolve their base types.
    if (SubType *subtype = dyn_cast<SubType>(A))
        baseTypeA = subtype->getTypeOf();
    if (SubType *subtype = dyn_cast<SubType>(B))
        baseTypeB = subtype->getTypeOf();

    return baseTypeA == baseTypeB;
}

bool TypeCheck::conversionRequired(Type *source, Type *target)
{
    if (source == target)
        return false;

    if (SubType *sourceSubTy = dyn_cast<SubType>(source)) {
        // If the target is the base type of the source a conversion is not
        // required.
        if (sourceSubTy->getTypeOf() == target)
            return false;

        // If the target is an unconstrained subtype of a common base, a
        // conversion is not needed.
        if (SubType *targetSubTy = dyn_cast<SubType>(target)) {
            if ((sourceSubTy->getTypeOf() == targetSubTy->getTypeOf()) &&
                !targetSubTy->isConstrained())
                return false;
        }
    }
    else if (SubType *targetSubTy = dyn_cast<SubType>(target)) {
        // If the target type is an unconstrained subtype of the source, a
        // conversion is not needed.
        if ((targetSubTy->getTypeOf() == source) &&
            !targetSubTy->isConstrained())
            return false;
    }
    return true;
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

    Resolver &resolver = scope->getResolver();
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
    Expr *externalNameExpr = cast_node<Expr>(externalNameNode);
    if (!checkExprInContext(externalNameExpr, resource.getTheStringType()))
        return;
    if (!externalNameExpr->isStaticStringExpr()) {
        report(externalNameExpr->getLocation(), diag::NON_STATIC_EXPRESSION);
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
    PragmaImport *pragma =
        new PragmaImport(pragmaLoc, ID, entity, externalNameExpr);
    srDecl->attachPragma(pragma);
}

PragmaAssert *TypeCheck::acceptPragmaAssert(Location loc, NodeVector &args)
{
    // Currently, assert pragmas take a single boolean valued argument.  The
    // parser knows this.
    assert(args.size() == 1 && "Wrong number of arguments for pragma Assert!");
    Expr *condition = cast_node<Expr>(args[0]);

    if (checkExprInContext(condition, resource.getTheBooleanType())) {
        // Get a string representing the source location of the assertion.
        //
        // FIXME: We should be calling a utility routine to parse the source
        // location.
        std::string message;
        SourceLocation sloc = getSourceLoc(loc);
        llvm::raw_string_ostream stream(message);
        std::string identity = sloc.getTextProvider()->getIdentity();
        stream << "Assertion failed at "
               << identity << ":"
               << sloc.getLine() << ":" << sloc.getColumn() << ".\n";
        return new PragmaAssert(loc, condition, stream.str());
    }
    return 0;;
}

