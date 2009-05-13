//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Stmt.h"
#include "llvm/ADT/DenseMap.h"
#include "TypeEqual.h"

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
      errorCount(0)
{
    populateInitialEnvironment();
}

TypeCheck::~TypeCheck() { }

// Called when then type checker is constructed.  Populates the top level scope
// with an initial environment.
void TypeCheck::populateInitialEnvironment()
{
    // Construct the Bool type as an enumeration, equivalent to a top level
    // declaration of the form "type Bool is (false, true);".  Note that false
    // is specified first, giving it the numeric value 0 (and true 1).
    IdentifierInfo *boolId  = resource.getIdentifierInfo("Bool");
    IdentifierInfo *trueId  = resource.getIdentifierInfo("true");
    IdentifierInfo *falseId = resource.getIdentifierInfo("false");
    IdentifierInfo *paramId = resource.getIdentifierInfo("|X|");

    EnumerationDecl    *boolEnum = new EnumerationDecl(boolId, 0, 0);
    EnumerationLiteral *falseLit = new EnumerationLiteral(boolEnum, falseId, 0);
    EnumerationLiteral *trueLit  = new EnumerationLiteral(boolEnum, trueId, 0);

    // Construct the Bool equality predicate.
    //
    // FIXME: This definition should be bound to "=", but we do not have infix
    // operator parsing yet.
    IdentifierInfo  *equalsId = resource.getIdentifierInfo("Equals");
    EnumerationType *boolType = boolEnum->getType();
    ParamValueDecl  *params[] = {
        new ParamValueDecl(paramId, boolType, MODE_DEFAULT, 0),
        new ParamValueDecl(paramId, boolType, MODE_DEFAULT, 0)
    };
    FunctionDecl *equals =
        new FunctionDecl(equalsId, 0, params, 2, boolType, 0);

    scope.addDirectDecl(boolEnum);
    scope.addDirectDecl(trueLit);
    scope.addDirectDecl(falseLit);
    scope.addDirectDecl(equals);

    theBoolDecl = boolEnum;
}

void TypeCheck::deleteNode(Node &node)
{
    Ast *ast = lift_node<Ast>(node);
    if (ast && ast->isDeletable()) delete ast;
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
    DeclarativeRegion *region = currentDeclarativeRegion();
    SubroutineDecl    *routine;

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

// Returns the % node for the current model, or 0 if we are not currently
// processing a model.
DomainType *TypeCheck::getCurrentPercent() const
{
    if (ModelDecl *model = getCurrentModel())
        return model->getPercent();
    return 0;
}

void TypeCheck::beginModelDeclaration(Descriptor &desc)
{
    assert((desc.isSignatureDescriptor() || desc.isDomainDescriptor()) &&
           "Beginning a model which is neither a signature or domain?");
    scope.push(MODEL_SCOPE);
}

void TypeCheck::endModelDefinition()
{
    assert(scope.getKind() == MODEL_SCOPE);
    ModelDecl *result = getCurrentModel();
    scope.pop();
    scope.addDirectModel(result);
    compUnit->addDeclaration(result);
}

Node TypeCheck::acceptModelParameter(Descriptor     &desc,
                                     IdentifierInfo *formal,
                                     Node            typeNode,
                                     Location        loc)
{
    ModelType *type = cast_node<ModelType>(typeNode);

    assert(scope.getKind() == MODEL_SCOPE);

    // Check that the parameter type denotes a signature.  For each parameter,
    // we create an AbstractDomainType to represent the formal, and add that
    // type into the current scope so that it may participate in upcomming
    // parameter types.
    if (SignatureType *sig = dyn_cast<SignatureType>(type)) {
        // Check that the formal does not duplicate any previous parameters and
        // does not shadow the model being defined.
        if (formal == desc.getIdInfo()) {
            report(loc, diag::MODEL_FORMAL_SHADOW) << formal;
            return getInvalidNode();
        }

        typedef Descriptor::paramIterator ParamIter;
        for (ParamIter iter = desc.beginParams();
             iter != desc.endParams(); ++iter) {
            AbstractDomainDecl *param = cast_node<AbstractDomainDecl>(*iter);
            if (param->getIdInfo() == formal) {
                report(loc, diag::DUPLICATE_FORMAL_PARAM) << formal;
                return getInvalidNode();
            }
        }

        typeNode.release();
        AbstractDomainDecl *dom = new AbstractDomainDecl(formal, sig, loc);
        scope.addDirectModel(dom);
        return getNode(dom);
    }
    else {
        report(loc, diag::NOT_A_SIGNATURE) << formal->getString();
        getCurrentModel()->markInvalid();
        return getInvalidNode();
    }
}

void TypeCheck::acceptModelDeclaration(Descriptor &desc)
{
    llvm::SmallVector<DomainType*, 4> domains;

    // Convert each parameter node into an AbstractDomainDecl.
    for (Descriptor::paramIterator iter = desc.beginParams();
         iter != desc.endParams(); ++iter) {
        AbstractDomainDecl *domain = cast_node<AbstractDomainDecl>(*iter);
        domains.push_back(domain->getType());
    }

    // Create the appropriate type of model.
    ModelDecl      *modelDecl;
    IdentifierInfo *percent = resource.getIdentifierInfo("%");
    IdentifierInfo *name    = desc.getIdInfo();
    Location        loc     = desc.getLocation();
    if (domains.empty()) {
        switch (desc.getKind()) {

        case Descriptor::DESC_Signature:
            modelDecl = new SignatureDecl(percent, name, loc);
            break;

        case Descriptor::DESC_Domain:
            modelDecl = new DomainDecl(percent, name, loc);
            break;

        default:
            assert(false && "Corruption of currentModelInfo->kind!");
        }
    }
    else {
        DomainType **formals = &domains[0];
        unsigned     arity   = domains.size();
        switch (desc.getKind()) {

        case Descriptor::DESC_Signature:
            modelDecl = new VarietyDecl(percent, name, loc, formals, arity);
            break;

        case Descriptor::DESC_Domain:
            modelDecl = new FunctorDecl(percent, name, loc, formals, arity);
            break;

        default:
            assert(false && "Corruption of currentModelInfo->kind!");
        }
    }

    currentModel      = modelDecl;
    declarativeRegion = modelDecl->asDeclarativeRegion();

    // Bring the model itself into scope, and release the nodes associated with
    // the given descriptor.
    scope.addDirectModel(modelDecl);
    desc.release();
}

void TypeCheck::acceptWithSupersignature(Node     typeNode,
                                         Location loc)
{
    ModelDecl     *model = getCurrentModel();
    Type          *type  = cast_node<Type>(typeNode);
    SignatureType *superSig;

    // Check that the node denotes a signature.
    superSig = dyn_cast<SignatureType>(type);
    if (!superSig) {
        report(loc, diag::NOT_A_SIGNATURE);
        return;
    }

    // Register the signature.
    typeNode.release();
    model->addDirectSignature(superSig);

    // Check that this signature does not introduce any conflicting type names
    // and bring all non-conflicting types into the current region.
    aquireSignatureTypeDeclarations(model, superSig->getDeclaration());
}

Node TypeCheck::acceptPercent(Location loc)
{
    ModelDecl *model = getCurrentModel();

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << "%";
        return getInvalidNode();
    }

    return getNode(model->getPercent());
}

// Returns true if the given type decl is equivalent to % in the context of the
// current domain.
bool TypeCheck::denotesDomainPercent(const TypeDecl *tyDecl)
{
    if (checkingDomain()) {
        DomainDecl *domain = getCurrentDomain();
        const DomainDecl *candidate = dyn_cast<DomainDecl>(tyDecl);
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
                                      Type **args, unsigned numArgs)
{
    assert(functor->getArity() == numArgs);

    if (checkingFunctor()) {
        FunctorDecl *currentFunctor = getCurrentFunctor();
        for (unsigned i = 0; i < numArgs; ++i) {
            DomainType *formal = currentFunctor->getFormalDomain(i);
            if (formal != args[i])
                return false;
        }
        return true;
    }
    return false;
}

Node TypeCheck::acceptTypeIdentifier(IdentifierInfo *id,
                                     Location        loc)
{
    TypeDecl   *type = scope.lookupType(id);
    const char *name = id->getString();

    if (type == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    switch (type->getKind()) {

    default:
        assert(false && "Cannot handle type declaration.");
        return getInvalidNode();

    case Ast::AST_DomainDecl:
        if (denotesDomainPercent(type)) {
            report(loc, diag::PERCENT_EQUIVALENT);
            return getNode(getCurrentPercent());
        }
        return getNode(type->getType());

    case Ast::AST_SignatureDecl:
    case Ast::AST_AbstractDomainDecl:
    case Ast::AST_CarrierDecl:
    case Ast::AST_EnumerationDecl:
        return getNode(type->getType());

    case Ast::AST_FunctorDecl:
    case Ast::AST_VarietyDecl:
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return getInvalidNode();
    }
}

Node TypeCheck::acceptTypeApplication(IdentifierInfo  *connective,
                                      NodeVector      &argumentNodes,
                                      Location        *argumentLocs,
                                      IdentifierInfo **keywords,
                                      Location        *keywordLocs,
                                      unsigned         numKeywords,
                                      Location         loc)
{
    ModelDecl  *model = scope.lookupDirectModel(connective);
    const char *name  = connective->getString();
    unsigned numArgs  = argumentNodes.size();

    assert(numKeywords <= numArgs && "More keywords than arguments!");

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    ParameterizedType *candidate =
        dyn_cast<ParameterizedType>(model->getType());

    if (!candidate || candidate->getArity() != numArgs) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return getInvalidNode();
    }

    unsigned numPositional = numArgs - numKeywords;
    llvm::SmallVector<Type*, 4> arguments(numArgs);

    // First, populate the argument vector with any positional parameters.
    for (unsigned i = 0; i < numPositional; ++i)
        arguments[i] = cast_node<Type>(argumentNodes[i]);

    // Process any keywords provided.
    for (unsigned i = 0; i < numKeywords; ++i) {
        IdentifierInfo *keyword    = keywords[i];
        Location        keywordLoc = keywordLocs[i];
        int             keywordIdx = candidate->getKeywordIndex(keyword);

        // Ensure the given keyword exists.
        if (keywordIdx < 0) {
            report(keywordLoc, diag::TYPE_HAS_NO_SUCH_KEYWORD)
                << keyword->getString() << candidate->getString();
                return getInvalidNode();
        }

        // The corresponding index of the keyword must be greater than the
        // number of supplied positional parameters (otherwise it would
        // `overlap' a positional parameter).
        if ((unsigned)keywordIdx < numPositional) {
            report(keywordLoc, diag::PARAM_PROVIDED_POSITIONALLY)
                << keyword->getString();
            return getInvalidNode();
        }

        // Ensure that this keyword is not a duplicate of any preceeding
        // keyword.
        for (unsigned j = 0; j < i; ++j) {
            if (keywords[j] == keyword) {
                report(keywordLoc, diag::DUPLICATE_KEYWORD)
                    << keyword->getString();
                return getInvalidNode();
            }
        }

        // Lift the argument node and add it to the set of arguments in its
        // proper position.
        Type *argument =
            cast_node<Type>(argumentNodes[i + numPositional]);
        argumentNodes[i + numPositional].release();
        arguments[keywordIdx] = argument;
    }

    // Check each argument type.
    for (unsigned i = 0; i < numArgs; ++i) {
        Type        *argument = arguments[i];
        Location       argLoc = argumentLocs[i];
        SignatureType *target =
            resolveArgumentType(candidate, &arguments[0], i);

        if (!checkType(argument, target, argLoc))
            return getInvalidNode();
    }

    // Obtain a memoized type node for this particular argument set.
    Node node = getInvalidNode();
    if (VarietyDecl *variety = dyn_cast<VarietyDecl>(model)) {
        node = getNode(variety->getCorrespondingType(&arguments[0], numArgs));
    }
    else {
        FunctorDecl *functor = cast<FunctorDecl>(model);

        // Cannonicalize type applications which are equivalent to `%'.
        if (denotesFunctorPercent(functor, &arguments[0], numArgs)) {
            report(loc, diag::PERCENT_EQUIVALENT);
            node = getNode(getCurrentPercent());
        }
        else {
            DomainInstanceDecl *instance =
                functor->getInstance(&arguments[0], numArgs);
            node = getNode(instance->getType());
        }
    }
    argumentNodes.release();
    return node;
}

void TypeCheck::beginWithExpression()
{
    // Nothing to do.  The declarative region of the current model is the
    // destination of all declarations in a with expression.
}

void TypeCheck::endWithExpression()
{
    // Ensure that all ambiguous declarations are redeclared.  For now, the only
    // ambiguity that can arise is wrt conflicting argument keyword sets.
    ModelDecl *model = getCurrentModel();
    ensureNecessaryRedeclarations(model);
}

// The following function resolves the argument type of a functor or variety
// given previous actual arguments.  That is, for a dependent argument list of
// the form (X : T, Y : U(X)), this function resolves the type of U(X) given an
// actual parameter for X.
SignatureType *TypeCheck::resolveArgumentType(ParameterizedType *type,
                                              Type             **actuals,
                                              unsigned           numActuals)
{
    AstRewriter rewriter;

    // For each actual argument, establish a map from the formal parameter to
    // the actual.
    for (unsigned i = 0; i < numActuals; ++i) {
        Type *formal = type->getFormalDomain(i);
        Type *actual = actuals[i];
        rewriter.addRewrite(formal, actual);
    }

    SignatureType *target = type->getFormalType(numActuals);
    return rewriter.rewrite(target);
}

// Creates a procedure or function decl depending on the kind of the
// supplied type.
SubroutineDecl *TypeCheck::makeSubroutineDecl(IdentifierInfo    *name,
                                              Location           loc,
                                              SubroutineType    *type,
                                              DeclarativeRegion *region)
{
    if (FunctionType *ftype = dyn_cast<FunctionType>(type))
        return new FunctionDecl(name, loc, ftype, region);

    ProcedureType *ptype = cast<ProcedureType>(type);
    return new ProcedureDecl(name, loc, ptype, region);
}

DomainType *TypeCheck::ensureDomainType(Node     node,
                                        Location loc) const
{
    if (Type *type = lift_node<Type>(node))
        return ensureDomainType(type, loc);
    report(loc, diag::NOT_A_DOMAIN);
    return 0;
}

DomainType *TypeCheck::ensureDomainType(Type    *type,
                                        Location loc) const
{
    if (DomainType *dom = dyn_cast<DomainType>(type))
        return dom;
    else if (CarrierType *carrier = dyn_cast<CarrierType>(type))
        return ensureDomainType(carrier->getRepresentationType(), loc);
    report(loc, diag::NOT_A_DOMAIN);
    return 0;
}

Type *TypeCheck::ensureValueType(Node     node,
                                 Location loc) const
{
    if (EnumerationType *etype = lift_node<EnumerationType>(node))
        return etype;
    else
        return ensureDomainType(node, loc);
}

namespace {

// This function is a helper for ensureNecessaryRedeclarations.  It searches all
// declarations present in the given declarative region for a match with respect
// to the given rewrites.  Returns a matching delcaration node or null.
SubroutineDecl *findDecl(const AstRewriter &rewrites,
                         DeclarativeRegion *region,
                         SubroutineDecl    *decl)
{
    typedef DeclarativeRegion::PredIter  PredIter;
    typedef DeclarativeRegion::PredRange PredRange;
    IdentifierInfo *name       = decl->getIdInfo();
    SubroutineType *targetType = decl->getType();
    PredRange       range      = region->findDecls(name);

    for (PredIter &iter = range.first; iter != range.second; ++iter) {
        if (SubroutineDecl *source = dyn_cast<SubroutineDecl>(*iter)) {
            SubroutineType *sourceType = source->getType();
            if (compareTypesUsingRewrites(rewrites, sourceType, targetType))
                return source;
        }
    }
    return 0;
}

} // End anonymous namespace.

void TypeCheck::ensureNecessaryRedeclarations(ModelDecl *model)
{
    // We scan the set of declarations for each direct signature of the given
    // model.  When a declaration is found which has not already been declared
    // we add it on good faith that all upcoming declarations will not conflict.
    //
    // When a conflict occurs (that is, when two declarations exists with the
    // same name and type but have disjoint keyword sets) we remember which
    // (non-direct) declarations in the model need an explicit redeclaration
    // using the badDecls set.  Once all the declarations are processed, we
    // remove those declarations found to be in conflict.
    typedef llvm::SmallPtrSet<SubroutineDecl*, 4> BadDeclSet;
    BadDeclSet badDecls;

    // An "indirect decl", in this context, is a subroutine decl which is
    // inherited from a super signature.  We maintain a map from such indirect
    // decls to the declaration node supplied by the signature.  This allows us
    // to provide diagnostics which mention the location of conflicts.
    typedef std::pair<SubroutineDecl*, SubroutineDecl*> IndirectPair;
    typedef llvm::DenseMap<SubroutineDecl*, SubroutineDecl*> IndirectDeclMap;
    IndirectDeclMap indirectDecls;

    SignatureSet          &sigset       = model->getSignatureSet();
    SignatureSet::iterator superIter    = sigset.beginDirect();
    SignatureSet::iterator endSuperIter = sigset.endDirect();
    for ( ; superIter != endSuperIter; ++superIter) {
        SignatureType *super   = *superIter;
        Sigoid        *sigdecl = super->getDeclaration();
        AstRewriter    rewrites;

        rewrites[sigdecl->getPercent()] = model->getPercent();
        rewrites.installRewrites(super);

        Sigoid::DeclIter iter    = sigdecl->beginDecls();
        Sigoid::DeclIter endIter = sigdecl->endDecls();
        for ( ; iter != endIter; ++iter) {
            if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*iter)) {
                IdentifierInfo *name   = srDecl->getIdInfo();
                SubroutineType *srType = srDecl->getType();
                SubroutineDecl *decl   = findDecl(rewrites, model, srDecl);

                // If a matching declaration was not found, apply the rewrites
                // and construct a new indirect declaration node for this
                // signature.
                if (!decl) {
                    SubroutineType *rewriteType = rewrites.rewrite(srType);
                    SubroutineDecl *rewriteDecl;

                    rewriteDecl = makeSubroutineDecl(name, 0, rewriteType, model);
                    rewriteDecl->setDeclarativeRegion(model);
                    model->addDecl(rewriteDecl);
                    indirectDecls.insert(IndirectPair(rewriteDecl, srDecl));
                } else if (indirectDecls.count(decl)) {
                    // An indirect declaration was found.  Since there is no
                    // overriding declaration in this case ensure that the
                    // keywords match.
                    SubroutineType *declType = decl->getType();

                    if (!declType->keywordsMatch(srType)) {
                        Location        modelLoc = model->getLocation();
                        SubroutineDecl *baseDecl = indirectDecls.lookup(decl);
                        SourceLocation     sloc1 =
                            getSourceLocation(baseDecl->getLocation());
                        SourceLocation     sloc2 =
                            getSourceLocation(srDecl->getLocation());
                        report(modelLoc, diag::MISSING_REDECLARATION)
                            << decl->getString() << sloc1 << sloc2;
                        badDecls.insert(decl);
                    }
                }
                // FIXME: The final case corresponds to a matching declaration
                // in a super signature which was redeclared.  Perhaps we should
                // explicity link the overriding declaration with those decls
                // which it overrides.
            }
        }

        // Remove and clean up memory for each inherited node found to require a
        // redeclaration.
        for (BadDeclSet::iterator iter = badDecls.begin();
             iter != badDecls.end(); ++iter) {
            SubroutineDecl *badDecl = *iter;
            model->removeDecl(badDecl);
            delete badDecl;
        }
    }
}

void TypeCheck::aquireSignatureTypeDeclarations(ModelDecl *model,
                                                Sigoid    *sigdecl)
{
    // Bring all type declarations provided by the given super signatures into
    // the current model.
    Sigoid::DeclIter iter    = sigdecl->beginDecls();
    Sigoid::DeclIter endIter = sigdecl->endDecls();

    for ( ; iter != endIter; ++iter) {
        if (TypeDecl *tyDecl = dyn_cast<TypeDecl>(*iter)) {
            if (ensureDistinctTypeDeclaration(model, tyDecl)) {
                model->addDecl(tyDecl);
                scope.addDirectDecl(tyDecl);
            }
        }
    }
}

bool TypeCheck::ensureDistinctTypeDeclaration(DeclarativeRegion *region,
                                              TypeDecl          *tyDecl)
{
    typedef DeclarativeRegion::PredRange PredRange;
    typedef DeclarativeRegion::PredIter  PredIter;
    IdentifierInfo *name  = tyDecl->getIdInfo();
    PredRange       range = region->findDecls(name);
    bool            allOK = true;

    if (range.first != range.second) {
        for (PredIter &dIter = range.first; dIter != range.second; ++dIter) {
            TypeDecl *conflict = dyn_cast<TypeDecl>(*dIter);
            if (conflict) {
                SourceLocation tyLoc =
                    getSourceLocation(tyDecl->getLocation());
                SourceLocation conflictLoc =
                    getSourceLocation(conflict->getLocation());
                report(tyDecl->getLocation(),
                       diag::CONFLICTING_TYPE_DECLS)
                    << name << tyLoc << conflictLoc;
                allOK = false;
                break;
            }
        }
    }
    return allOK;
}

bool TypeCheck::acceptObjectDeclaration(Location        loc,
                                        IdentifierInfo *name,
                                        Node            typeNode,
                                        Node            initializerNode)
{
    Type *type = cast_node<Type>(typeNode);

    if ((type = ensureValueType(typeNode, loc))) {
        ObjectDecl *decl = new ObjectDecl(name, type, loc);

        if (!initializerNode.isNull()) {
            Expr *expr       = cast_node<Expr>(initializerNode);
            Type *targetType = decl->getType();
            if (checkType(expr, targetType)) {
                initializerNode.release();
                decl->setInitializer(expr);
            }
            else
                return false;
        }
        typeNode.release();
        scope.addDirectValue(decl);
        return true;
    }
    return false;
}

bool TypeCheck::acceptImportDeclaration(Node importedNode, Location loc)
{
    Type         *type = cast_node<Type>(importedNode);
    DomainType   *domain;

    if (CarrierType *carrier = dyn_cast<CarrierType>(type))
        domain = dyn_cast<DomainType>(carrier->getRepresentationType());
    else
        domain = dyn_cast<DomainType>(type);

    if (!domain) {
        report(loc, diag::IMPORT_FROM_NON_DOMAIN);
        return false;
    }

    importedNode.release();
    scope.addImport(domain);

    // FIXME:  We need to stitch this import declaration into the current
    // context.
    new ImportDecl(domain, loc);
    return true;
}

void TypeCheck::beginAddExpression()
{
    Domoid *domoid = getCurrentDomoid();
    assert(domoid && "Processing `add' expression outside domain context!");

    // Switch to the declarative region which this domains AddDecl provides.
    declarativeRegion = domoid->getImplementation();
    assert(declarativeRegion && "Domain missing Add declaration node!");

    // Enter a new scope for the add expression.
    scope.push();
}

void TypeCheck::endAddExpression()
{
    // Leave the scope corresponding to the add expression and switch back to
    // the declarative region of the defining domain.
    declarativeRegion = declarativeRegion->getParent();
    assert(declarativeRegion == getCurrentModel()->asDeclarativeRegion());
    scope.pop();
}

void TypeCheck::acceptCarrier(IdentifierInfo *name, Node typeNode, Location loc)
{
    // We should always be in an add declaration.
    AddDecl *add = cast<AddDecl>(declarativeRegion);

    if (add->hasCarrier()) {
        report(loc, diag::MULTIPLE_CARRIER_DECLARATIONS);
        return;
    }

    if (Type *type = ensureValueType(typeNode, loc)) {
        typeNode.release();
        CarrierDecl *carrier = new CarrierDecl(name, type, loc);
        add->setCarrier(carrier);
        scope.addDirectDecl(carrier);
    }
}

// There is nothing for us to do at the start of a subroutine declaration.
// Creation of the declaration itself is deferred until
// acceptSubroutineDeclaration is called.
void TypeCheck::beginSubroutineDeclaration(Descriptor &desc)
{
    assert((desc.isFunctionDescriptor() || desc.isProcedureDescriptor()) &&
           "Beginning a subroutine which is neither a function or procedure?");
}

Node TypeCheck::acceptSubroutineParameter(IdentifierInfo   *formal,
                                          Location          loc,
                                          Node              typeNode,
                                          ParameterMode     mode)
{
    // FIXME: The location provided here is the location of the formal, not the
    // location of the type.  The type node here should be a ModelRef or similar
    // which encapsulates the needed location information.
    Type *dom = ensureValueType(typeNode, loc);

    if (!dom) return getInvalidNode();

    typeNode.release();
    ParamValueDecl *paramDecl = new ParamValueDecl(formal, dom, mode, loc);
    return getNode(paramDecl);
}

Node TypeCheck::acceptSubroutineDeclaration(Descriptor &desc,
                                            bool        definitionFollows)
{
    assert((desc.isFunctionDescriptor() || desc.isProcedureDescriptor()) &&
           "Descriptor does not denote a subroutine!");

    // If we uncover a problem with the subroutines parameters, the following
    // flag is set to false.  Note that we do not create a declaration for the
    // subroutine unless it checks out 100%.
    bool paramsOK = true;

    // Every parameter of this descriptor should be a ParamValueDecl.  As we
    // validate the type of each node, test that no duplicate formal parameters
    // are accumulated.  Any duplicates found are discarded.
    typedef llvm::SmallVector<ParamValueDecl*, 6> paramVec;
    paramVec parameters;
    if (desc.hasParams()) {
        for (Descriptor::paramIterator iter = desc.beginParams();
             iter != desc.endParams(); ++iter) {

            ParamValueDecl *param = cast_node<ParamValueDecl>(*iter);

            for (paramVec::iterator cursor = parameters.begin();
                 cursor != parameters.end(); ++cursor) {
                if (param->getIdInfo() == (*cursor)->getIdInfo()) {
                    report(param->getLocation(), diag::DUPLICATE_FORMAL_PARAM)
                        << param->getString();
                    paramsOK = false;
                }
            }

            // If this is a function descriptor, check that the parameter mode
            // is not of an "out" variety.
            if (desc.isFunctionDescriptor()
                && (param->getParameterMode() == MODE_OUT ||
                    param->getParameterMode() == MODE_IN_OUT)) {
                report(param->getLocation(), diag::OUT_MODE_IN_FUNCTION);
                paramsOK = false;
            }

            // Add the parameter to the set if checking is proceeding smoothly.
            if (paramsOK) parameters.push_back(param);
        }
    }

    SubroutineDecl *routineDecl = 0;
    DeclarativeRegion   *region = currentDeclarativeRegion();
    IdentifierInfo        *name = desc.getIdInfo();
    Location           location = desc.getLocation();

    if (desc.isFunctionDescriptor()) {
        // If this descriptor is a function, then we must return a value type.
        Type *returnType = ensureValueType(desc.getReturnType(), 0);
        if (returnType && paramsOK)
            routineDecl = new FunctionDecl(name,
                                           location,
                                           &parameters[0],
                                           parameters.size(),
                                           returnType,
                                           region);
    }
    else if (paramsOK)
        routineDecl = new ProcedureDecl(name,
                                        location,
                                        &parameters[0],
                                        parameters.size(),
                                        region);

    if (!routineDecl) return getInvalidNode();

    // Check that this declaration does not conflict with any other.
    if (Decl *extantDecl = region->findDecl(name, routineDecl->getType())) {
        SubroutineDecl *sdecl = cast<SubroutineDecl>(extantDecl);
        SourceLocation   sloc = getSourceLocation(extantDecl->getLocation());

        if (!sdecl->hasBody() && definitionFollows)
            sdecl->setDefiningDeclaration(routineDecl);
        else {
            report(location, diag::SUBROUTINE_REDECLARATION)
                << routineDecl->getString()
                << sloc;
            return getInvalidNode();
        }
    }
    else {
        // Add the subroutine declaration into the current declarative region.
        region->addDecl(routineDecl);
        scope.addDirectSubroutine(routineDecl);
    }

    // Since the declaration has been added permanently to the environment,
    // ensure the returned Node does not reclaim the decl.
    Node routine = getNode(routineDecl);
    routine.release();
    desc.release();
    return routine;
}

void TypeCheck::beginSubroutineDefinition(Node declarationNode)
{
    SubroutineDecl *srDecl = cast_node<SubroutineDecl>(declarationNode);
    declarationNode.release();

    // Enter a scope for the subroutine definition and populate it with the
    // formal parmeter bindings.
    scope.push(FUNCTION_SCOPE);
    typedef SubroutineDecl::ParamDeclIterator ParamIter;
    for (ParamIter iter = srDecl->beginParams();
         iter != srDecl->endParams(); ++iter) {
        ParamValueDecl *param = *iter;
        scope.addDirectValue(param);
    }

    // Allocate a BlockStmt for the subroutines body and make this block the
    // current declarative region.
    assert(!srDecl->hasBody() && "Current subroutine already has a body!");
    BlockStmt *block = new BlockStmt(0, srDecl, srDecl->getIdInfo());
    srDecl->setBody(block);
    declarativeRegion = block;
}

void TypeCheck::acceptSubroutineStmt(Node stmt)
{
    SubroutineDecl *subroutine = getCurrentSubroutine();
    assert(subroutine && "No currnet subroutine!");

    stmt.release();
    BlockStmt *block = subroutine->getBody();
    block->addStmt(cast_node<Stmt>(stmt));
}

void TypeCheck::endSubroutineDefinition()
{
    assert(scope.getKind() == FUNCTION_SCOPE);

    // We established two levels of declarative regions in
    // beginSubroutineDefinition: one for the BlockStmt constituting the body
    // and another corresponding the subroutine itself.  Remove them both.
    declarativeRegion = declarativeRegion->getParent()->getParent();
    scope.pop();
}

Node TypeCheck::acceptKeywordSelector(IdentifierInfo *key,
                                      Location        loc,
                                      Node            exprNode,
                                      bool            forSubroutine)
{
    if (!forSubroutine) {
        assert(false && "cannot accept keyword selectors for types yet!");
        return getInvalidNode();
    }

    exprNode.release();
    Expr *expr = cast_node<Expr>(exprNode);
    return getNode(new KeywordSelector(key, loc, expr));
}

Node TypeCheck::acceptEnumerationType(IdentifierInfo *name, Location loc)
{
    DeclarativeRegion *region      = currentDeclarativeRegion();
    EnumerationDecl   *enumeration = new EnumerationDecl(name, loc, region);

    if (ensureDistinctTypeDeclaration(region, enumeration)) {
        region->addDecl(enumeration);
        scope.addDirectDecl(enumeration);
        Node result = getNode(enumeration);
        result.release();
        return result;
    }
    else
        return getInvalidNode();
}

void TypeCheck::acceptEnumerationLiteral(Node            enumerationNode,
                                         IdentifierInfo *name,
                                         Location        loc)
{
    EnumerationDecl *enumeration = cast_node<EnumerationDecl>(enumerationNode);

    if (enumeration->containsDecl(name)) {
        report(loc, diag::MULTIPLE_ENUMERATION_LITERALS) << name;
        return;
    }

    EnumerationLiteral *lit = new EnumerationLiteral(enumeration, name, loc);
    scope.addDirectDecl(lit);
}

bool TypeCheck::checkType(Type *source, SignatureType *target, Location loc)
{
    if (DomainType *domain = dyn_cast<DomainType>(source)) {
        if (!has(domain, target)) {
            report(loc, diag::DOES_NOT_SATISFY)
                << domain->getString()  << target->getString();
            return false;
        }
        return true;
    }

    if (CarrierType *carrier = dyn_cast<CarrierType>(source)) {
        DomainType *rep =
            dyn_cast<DomainType>(carrier->getRepresentationType());
        if (!rep) {
            report(loc, diag::NOT_A_DOMAIN);
            return false;
        }
        if (!has(rep, target)) {
            report(loc, diag::DOES_NOT_SATISFY)
                << carrier->getString() << target->getString();
            return false;
        }
        return true;
    }

    // Otherwise, the source does not denote a domain, and so cannot satisfy the
    // signature constraint.
    report(loc, diag::NOT_A_DOMAIN);
    return false;
}

bool TypeCheck::checkType(Expr *expr, Type *targetType)
{
    if (expr->hasType()) {
        if (targetType->equals(expr->getType()))
            return true;
        report(expr->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }
    else {
        // Otherwise, the expression must be a function call overloaded on the
        // return type.
        FunctionCallExpr *fcall = cast<FunctionCallExpr>(expr);
        return resolveFunctionCall(fcall, targetType);
    }
}
