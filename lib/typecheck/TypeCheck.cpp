//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DeclProducer.h"
#include "Scope.h"
#include "TypeEqual.h"

#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Stmt.h"

#include "llvm/ADT/DenseMap.h"

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
      errorCount(0),
      declProducer(new DeclProducer(&resource))
{
    populateInitialEnvironment();
}

TypeCheck::~TypeCheck()
{
    delete scope;
    delete declProducer;
}

// Called when then type checker is constructed.  Populates the top level scope
// with an initial environment.
void TypeCheck::populateInitialEnvironment()
{
    EnumerationDecl *theBoolDecl = declProducer->getBoolDecl();
    scope->addDirectDecl(theBoolDecl);
    importDeclRegion(theBoolDecl);

    IntegerDecl *theIntegerDecl = declProducer->getIntegerDecl();
    scope->addDirectDecl(theIntegerDecl);
    importDeclRegion(theIntegerDecl);
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
    scope->push(MODEL_SCOPE);
}

void TypeCheck::endModelDefinition()
{
    assert(scope->getKind() == MODEL_SCOPE);
    ModelDecl *result = getCurrentModel();
    scope->pop();
    scope->addDirectModel(result);
    compUnit->addDeclaration(result);
}

Node TypeCheck::acceptModelParameter(Descriptor &desc, IdentifierInfo *formal,
                                     Node typeNode, Location loc)
{
    assert(scope->getKind() == MODEL_SCOPE);

    Type *type = cast_node<Type>(typeNode);

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

        if (!checkDescriptorDuplicateParams(desc, formal, loc))
            return getInvalidNode();

        typeNode.release();
        AbstractDomainDecl *dom = new AbstractDomainDecl(formal, sig, loc);
        scope->addDirectDecl(dom);
        return getNode(dom);
    }
    else {
        report(loc, diag::NOT_A_SIGNATURE);
        return getInvalidNode();
    }
}

void TypeCheck::acceptModelDeclaration(Descriptor &desc)
{
    // Create the appropriate type of model.
    ModelDecl *modelDecl;
    IdentifierInfo *name = desc.getIdInfo();
    Location loc = desc.getLocation();
    if (desc.hasParams()) {
        // Convert each parameter node into an AbstractDomainDecl.
        llvm::SmallVector<AbstractDomainDecl*, 4> domains;
        convertDescriptorParams<AbstractDomainDecl>(desc, domains);
        AbstractDomainDecl **formals = &domains[0];
        unsigned arity = domains.size();
        switch (desc.getKind()) {

        case Descriptor::DESC_Signature:
            modelDecl = new VarietyDecl(resource, name, loc, formals, arity);
            break;

        case Descriptor::DESC_Domain:
            modelDecl = new FunctorDecl(resource, name, loc, formals, arity);
            break;

        default:
            assert(false && "Corruption of currentModelInfo->kind!");
        }
    }
    else {
        switch (desc.getKind()) {

        case Descriptor::DESC_Signature:
            modelDecl = new SignatureDecl(resource, name, loc);
            break;

        case Descriptor::DESC_Domain:
            modelDecl = new DomainDecl(resource, name, loc);
            break;

        default:
            assert(false && "Corruption of currentModelInfo->kind!");
        }
    }

    declarativeRegion = modelDecl->asDeclRegion();

    // For each parameter node, set its declarative region to be that of the
    // newly constructed model.
    for (Descriptor::paramIterator iter = desc.beginParams();
         iter != desc.endParams(); ++iter) {
        AbstractDomainDecl *domain = cast_node<AbstractDomainDecl>(*iter);
        domain->setDeclRegion(declarativeRegion);
    }

    // Bring the model itself into scope, and release the nodes associated with
    // the given descriptor.
    currentModel = modelDecl;
    scope->addDirectModel(modelDecl);
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
    aquireSignatureTypeDeclarations(model, superSig->getSigoid());
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

// Returns true if the given decl is equivalent to % in the context of the
// current domain.
bool TypeCheck::denotesDomainPercent(const Decl *decl)
{
    if (checkingDomain()) {
        DomainDecl *domain = getCurrentDomain();
        const DomainDecl *candidate = dyn_cast<DomainDecl>(decl);
        if (candidate and domain)
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
            DomainType *formal = currentFunctor->getFormalType(i);
            if (formal != args[i])
                return false;
        }
        return true;
    }
    return false;
}

/// Resolves the argument type of a Functor or Variety given previous actual
/// arguments.
///
/// For a dependent argument list of the form <tt>(X : T, Y : U(X))</tt>, this
/// function resolves the type of \c U(X) given an actual parameter for \c X.
/// It is assumed that the actual arguments provided are compatable with the
/// given model.
SignatureType *
TypeCheck::resolveFormalSignature(ModelDecl *parameterizedModel,
                                  Type **arguments, unsigned numArguments)
{
    assert(parameterizedModel->isParameterized());
    assert(numArguments < parameterizedModel->getArity());

    AstRewriter rewriter;

    // For each actual argument, establish a map from the formal parameter to
    // the actual.
    for (unsigned i = 0; i < numArguments; ++i) {
        Type *formal = parameterizedModel->getFormalType(i);
        Type *actual = arguments[i];
        rewriter.addRewrite(formal, actual);
    }

    SignatureType *target = parameterizedModel->getFormalSignature(numArguments);
    return rewriter.rewrite(target);
}

Node TypeCheck::acceptTypeName(IdentifierInfo *id, Location loc, Node qualNode)
{
    Decl *decl = 0;

    if (!qualNode.isNull()) {
        Qualifier *qualifier = cast_node<Qualifier>(qualNode);
        DeclRegion *region = qualifier->resolve();
        DeclRegion::PredRange range = region->findDecls(id);

        // Search the region for a type of the given name.  Type names do not
        // overload so if the type exists, it is unique, and the first match is
        // accepted.
        for (DeclRegion::PredIter iter = range.first;
             iter != range.second; ++iter) {
            Decl *candidate = *iter;
            if ((decl = dyn_cast<ModelDecl>(candidate)) or
                (decl = dyn_cast<TypedDecl>(candidate)))
                break;
        }
    }
    else {
        // FIXME: We should have a unified lookup method for this.
        decl = scope->lookupModel(id);
        if (!decl)
            decl = scope->lookupType(id);
    }

    if (decl == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << id;
        return getInvalidNode();
    }

    switch (decl->getKind()) {

    default:
        assert(false && "Cannot handle type declaration.");
        return getInvalidNode();

    case Ast::AST_DomainDecl: {
        if (denotesDomainPercent(decl)) {
            report(loc, diag::PERCENT_EQUIVALENT);
            return getNode(getCurrentPercent());
        }
        DomainDecl *domDecl = cast<DomainDecl>(decl);
        return getNode(domDecl->getInstance()->getType());
    }

    case Ast::AST_SignatureDecl: {
        SignatureDecl *sigDecl = cast<SignatureDecl>(decl);
        return getNode(sigDecl->getCorrespondingType());
    }

    case Ast::AST_AbstractDomainDecl:
    case Ast::AST_CarrierDecl:
    case Ast::AST_EnumerationDecl:
    case Ast::AST_IntegerDecl: {
        TypedDecl *tyDecl = cast<TypedDecl>(decl);
        return getNode(tyDecl->getType());
    }

    case Ast::AST_FunctorDecl:
    case Ast::AST_VarietyDecl:
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << id;
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
    ModelDecl *model = scope->lookupModel(connective);
    const char *name  = connective->getString();
    unsigned numArgs  = argumentNodes.size();

    assert(numKeywords <= numArgs && "More keywords than arguments!");

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    if (!model->isParameterized() || model->getArity() != numArgs) {
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
        IdentifierInfo *keyword = keywords[i];
        Location keywordLoc = keywordLocs[i];
        int keywordIdx = model->getKeywordIndex(keyword);

        // Ensure the given keyword exists.
        if (keywordIdx < 0) {
            report(keywordLoc, diag::TYPE_HAS_NO_SUCH_KEYWORD)
                << keyword->getString() << name;
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
        Type *argument = arguments[i];
        Location argLoc = argumentLocs[i];
        SignatureType *target = resolveFormalSignature(model, &arguments[0], i);
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

// Creates a procedure or function decl depending on the kind of the
// supplied type.
SubroutineDecl *TypeCheck::makeSubroutineDecl(IdentifierInfo *name,
                                              Location        loc,
                                              SubroutineType *type,
                                              DeclRegion     *region)
{
    if (FunctionType *ftype = dyn_cast<FunctionType>(type))
        return new FunctionDecl(name, loc, ftype, region);

    ProcedureType *ptype = cast<ProcedureType>(type);
    return new ProcedureDecl(name, loc, ptype, region);
}

DomainType *TypeCheck::ensureDomainType(Node node,
                                        Location loc,
                                        bool report)
{
    Type *type = cast_node<Type>(node);
    return ensureDomainType(type, loc);
}

DomainType *TypeCheck::ensureDomainType(Type *type,
                                        Location loc,
                                        bool report)
{
    if (DomainType *dom = dyn_cast<DomainType>(type))
        return dom;
    else if (CarrierType *carrier = dyn_cast<CarrierType>(type))
        return ensureDomainType(carrier->getRepresentationType(), loc);
    if (report)
        this->report(loc, diag::NOT_A_DOMAIN);
    return 0;
}

Type *TypeCheck::ensureValueType(Type *type,
                                 Location loc,
                                 bool report)
{
    if (isa<TypedefType>(type) ||
        isa<EnumerationType>(type) ||
        isa<CarrierType>(type) ||
        ensureDomainType(type, loc, false))
        return type;
    if (report)
        this->report(loc, diag::TYPE_CANNOT_DENOTE_VALUE);
    return 0;
}

Type *TypeCheck::ensureValueType(Node node,
                                 Location loc,
                                 bool report)
{
    Type *type = cast_node<Type>(node);
    return ensureValueType(type, loc, report);
}

/// Returns true if \p expr is a static integer expression.  If so, initializes
/// \p result to a signed value which can accommodate the given static
/// expression.
bool TypeCheck::ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result)
{
    // FIXME: IntegerLiterals are not the only kind of static integer
    // expression!
    if (IntegerLiteral *ILit = dyn_cast<IntegerLiteral>(expr)) {
        result = ILit->getValue();
        return true;
    }
    else {
        report(expr->getLocation(), diag::NON_STATIC_EXPRESSION);
        return false;
    }
}

// Search all declarations present in the given declarative region for a match
// with respect to the given rewrites.  Returns a matching delcaration node or
// null.
SubroutineDecl *TypeCheck::findDecl(const AstRewriter &rewrites,
                                    DeclRegion        *region,
                                    SubroutineDecl    *decl)
{
    typedef DeclRegion::PredIter  PredIter;
    typedef DeclRegion::PredRange PredRange;
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
        Sigoid        *sigdecl = super->getSigoid();
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
                // model.  Also, set the origin of this new decl to point at the
                // node from which it was derived.
                if (!decl) {
                    SubroutineType *rewriteType = rewrites.rewrite(srType);
                    SubroutineDecl *rewriteDecl;

                    rewriteDecl = makeSubroutineDecl(name, 0, rewriteType, model);
                    rewriteDecl->setOrigin(srDecl);
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
                            getSourceLoc(baseDecl->getLocation());
                        SourceLocation     sloc2 =
                            getSourceLoc(srDecl->getLocation());
                        report(modelLoc, diag::MISSING_REDECLARATION)
                            << decl->getString() << sloc1 << sloc2;
                        badDecls.insert(decl);
                    }
                }
                else {
                    // A declaration provided by a supersignature was found to
                    // exactly match one provided by the current model.  Set the
                    // origin to point at this decl if it has not been set
                    // already.
                    //
                    // FIXME: We should warn here that the declaration is
                    // redundant.
                    if (!decl->hasOrigin())
                        decl->setOrigin(srDecl);
                }
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
    // Bring all type declarations provided by the signature into the given
    // model.
    DeclRegion::DeclIter iter    = sigdecl->beginDecls();
    DeclRegion::DeclIter endIter = sigdecl->endDecls();

    for ( ; iter != endIter; ++iter) {
        if (TypedDecl *tyDecl = dyn_cast<TypedDecl>(*iter)) {
            IdentifierInfo *name = tyDecl->getIdInfo();
            Location loc = tyDecl->getLocation();
            if (ensureDistinctTypeName(name, loc, model)) {
                model->addDecl(tyDecl);
                scope->addDirectDecl(tyDecl);

                if (EnumerationDecl *decl = dyn_cast<EnumerationDecl>(tyDecl)) {
                    DeclRegion::DeclIter litIter = decl->beginDecls();
                    DeclRegion::DeclIter litEnd  = decl->endDecls();
                    for ( ; litIter != litEnd; ++litIter)
                        scope->addDirectDecl(*litIter);
                }
            }
        }
    }
}

/// Returns true if the given identifier can be used to name a new type within
/// the context of the given declarative region.  Otherwise, false is returned
/// and diagnostics are posted with respect to the given location.
bool TypeCheck::ensureDistinctTypeName(IdentifierInfo *name, Location loc,
                                       DeclRegion *region)
{
    typedef DeclRegion::PredRange PredRange;
    typedef DeclRegion::PredIter  PredIter;
    PredRange range = region->findDecls(name);

    if (range.first != range.second) {
        for (PredIter &iter = range.first; iter != range.second; ++iter) {
            TypedDecl *conflict = dyn_cast<TypedDecl>(*iter);
            if (conflict) {
                SourceLocation sloc = getSourceLoc(conflict->getLocation());
                report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
                return false;
            }
        }
    }

    // If the declarative region denotes a model or an AddDecl, check to ensure
    // that the name does not conflict with the model name or any model
    // parameters.  We do not need to check declarations inherited from super
    // signatures as they are injected into the models declarative region.
    Ast *ast = region->asAst();
    if (AddDecl *add = dyn_cast<AddDecl>(ast)) {
        // Recursively check the declarations present in this AddDecls
        // associated model.
        ModelDecl *model = add->getImplementedDomoid();
        return ensureDistinctTypeName(name, loc, model);
    }

    if (ModelDecl *model = dyn_cast<ModelDecl>(ast)) {
        if (name == model->getIdInfo()) {
            SourceLocation sloc = getSourceLoc(model->getLocation());
            report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
            return false;
        }

        if (model->isParameterized()) {
            for (unsigned i = 0; i < model->getArity(); ++i) {
                IdentifierInfo *id = model->getFormalIdInfo(i);
                if (name == id) {
                    AbstractDomainDecl *formal =
                        model->getFormalType(i)->getAbstractDecl();
                    SourceLocation sloc = getSourceLoc(formal->getLocation());
                    report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
                    return false;
                }
            }
        }
    }
    return true;
}

bool TypeCheck::acceptObjectDeclaration(Location        loc,
                                        IdentifierInfo *name,
                                        Node            typeNode,
                                        Node            initializerNode)
{
    Type *type = ensureValueType(typeNode, loc);

    if (!type) return false;

    // Check that this declaration does not conflict.
    for (DeclRegion *region = currentDeclarativeRegion();
         region != 0; region = region->getParent()) {
        DeclRegion::PredRange range = region->findDecls(name);
        if (range.first != range.second) {
            Decl *decl = *range.first;
            SourceLocation sloc = getSourceLoc(decl->getLocation());
            report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
            return false;
        }

        Ast *ast = region->asAst();
        if (SubroutineDecl *decl = dyn_cast<SubroutineDecl>(ast)) {
            // Check that the name does not conflict with the formal parameters
            // of the subroutine.
            for (SubroutineDecl::ParamDeclIterator iter = decl->beginParams();
                 iter != decl->endParams(); ++iter) {
                ParamValueDecl *param = *iter;
                if (name == param->getIdInfo()) {
                    SourceLocation sloc = getSourceLoc(param->getLocation());
                    report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
                    return false;
                }
            }
            break;
        }

        if (isa<ModelDecl>(ast))
            break;
    }

    ObjectDecl *decl;
    if (initializerNode.isNull())
        decl = new ObjectDecl(name, type, loc);
    else {
        Expr *expr = cast_node<Expr>(initializerNode);
        if (checkExprInContext(expr, type)) {
            decl = new ObjectDecl(name, type, loc);
            decl->setInitializer(expr);
        }
        else
            return false;
    }

    typeNode.release();
    initializerNode.release();
    scope->addDirectValue(decl);
    currentDeclarativeRegion()->addDecl(decl);
    return true;
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
    scope->addImport(domain);

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
    scope->push();
}

void TypeCheck::endAddExpression()
{
    ensureExportConstraints(getCurrentDomoid()->getImplementation());

    // Leave the scope corresponding to the add expression and switch back to
    // the declarative region of the defining domain.
    declarativeRegion = declarativeRegion->getParent();
    assert(declarativeRegion == getCurrentModel()->asDeclRegion());
    scope->pop();
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
        scope->addDirectDecl(carrier);
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

Node TypeCheck::acceptSubroutineParameter(IdentifierInfo *formal, Location loc,
                                          Node typeNode, PM::ParameterMode mode)
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
    //
    // Start by ensuring all parameters are distinct.
    bool paramsOK = checkDescriptorDuplicateParams(desc);
    IdentifierInfo *name = desc.getIdInfo();
    Location location = desc.getLocation();

    // Every parameter of this descriptor should be a ParamValueDecl.  As we
    // validate the type of each node, test that no duplicate formal parameters
    // are accumulated.  Any duplicates found are discarded.
    typedef llvm::SmallVector<ParamValueDecl*, 6> paramVec;
    paramVec parameters;
    convertDescriptorParams<ParamValueDecl>(desc, parameters);

    // If this is a function descriptor, ensure that every parameter is of mode
    // "in".  Also, ensure that if this function names a binary operator it has
    // arity 2.
    if (desc.isFunctionDescriptor()) {
        for (paramVec::iterator I = parameters.begin();
             I != parameters.end(); ++I)
            paramsOK = checkFunctionParameter(*I);

        if (parameters.size() != 2 and namesBinaryFunction(desc.getIdInfo())) {
            report(location, diag::BINARY_FUNCTION_ARITY_MISMATCH) << name;
            paramsOK = false;
        }
    }

    // If the parameters did not check, stop.
    if (!paramsOK)
        return getInvalidNode();

    SubroutineDecl *routineDecl = 0;
    DeclRegion *region = currentDeclarativeRegion();

    if (desc.isFunctionDescriptor()) {
        if (Type *returnType = ensureValueType(desc.getReturnType(), 0)) {
            routineDecl = new FunctionDecl(name,
                                           location,
                                           &parameters[0],
                                           parameters.size(),
                                           returnType,
                                           region);
        }
    }
    else {
        routineDecl = new ProcedureDecl(name,
                                        location,
                                        &parameters[0],
                                        parameters.size(),
                                        region);
    }

    if (!routineDecl) return getInvalidNode();

    // Check that this declaration does not conflict with any other in the
    // current region.  If we find a decl with the same name and type, it must
    // be a forward decl (detected by checking that there no associated body).
    // When a forward decl is present, and a definition follows, set the
    // defining declaration link in the forward decl.
    if (Decl *extantDecl = region->findDecl(name, routineDecl->getType())) {
        SubroutineDecl *sdecl = cast<SubroutineDecl>(extantDecl);
        SourceLocation sloc = getSourceLoc(extantDecl->getLocation());

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
        scope->addDirectSubroutine(routineDecl);
    }

    // This is an immediate declaration.  Mark it so.
    routineDecl->setImmediate();

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
    scope->push(FUNCTION_SCOPE);
    typedef SubroutineDecl::ParamDeclIterator ParamIter;
    for (ParamIter iter = srDecl->beginParams();
         iter != srDecl->endParams(); ++iter) {
        ParamValueDecl *param = *iter;
        scope->addDirectValue(param);
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
    assert(scope->getKind() == FUNCTION_SCOPE);

    // We established two levels of declarative regions in
    // beginSubroutineDefinition: one for the BlockStmt constituting the body
    // and another corresponding to the subroutine itself.  Pop them both.
    declarativeRegion = declarativeRegion->getParent()->getParent();
    scope->pop();
}

Node TypeCheck::acceptKeywordSelector(IdentifierInfo *key, Location loc,
                                      Node exprNode, bool forSubroutine)
{
    if (!forSubroutine) {
        assert(false && "cannot accept keyword selectors for types yet!");
        return getInvalidNode();
    }

    exprNode.release();
    Expr *expr = cast_node<Expr>(exprNode);
    return getNode(new KeywordSelector(key, loc, expr));
}

Node TypeCheck::beginEnumerationType(IdentifierInfo *name, Location loc)
{
    DeclRegion *region = currentDeclarativeRegion();

    if (!ensureDistinctTypeName(name, loc, region))
        return getInvalidNode();

    EnumerationDecl *enumeration = new EnumerationDecl(name, loc, region);
    return getNode(enumeration);
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
    new EnumLiteral(enumeration, name, loc);
}

void TypeCheck::endEnumerationType(Node enumerationNode)
{
    DeclRegion *region = currentDeclarativeRegion();
    EnumerationDecl *enumeration = cast_node<EnumerationDecl>(enumerationNode);

    enumerationNode.release();
    declProducer->createImplicitDecls(enumeration);
    region->addDecl(enumeration);
    importDeclRegion(enumeration);
    scope->addDirectDecl(enumeration);
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

    if (!ensureDistinctTypeName(name, loc, region))
        return;

    Expr *lowExpr = cast_node<Expr>(lowNode);
    Expr *highExpr = cast_node<Expr>(highNode);

    llvm::APInt lowValue;
    llvm::APInt highValue;
    if (!ensureStaticIntegerExpr(lowExpr, lowValue) or
        !ensureStaticIntegerExpr(highExpr, highValue))
        return;

    // Sign extend the values so that they have identical widths.
    unsigned lowWidth = lowValue.getBitWidth();
    unsigned highWidth = highValue.getBitWidth();
    if (lowWidth < highWidth)
        lowValue.sext(highWidth);
    else if (highWidth < lowWidth)
        highValue.sext(lowWidth);

    // Obtain a uniqued integer type to represent the base type of this
    // declaration and release the range expressions as they are now owned by
    // this new declaration.
    lowNode.release();
    highNode.release();
    IntegerType *intTy = resource.getIntegerType(lowValue, highValue);
    IntegerDecl *Idecl =
        new IntegerDecl(name, loc, lowExpr, highExpr, intTy, region);

    declProducer->createImplicitDecls(Idecl);
    region->addDecl(Idecl);
    importDeclRegion(Idecl);
    scope->addDirectDecl(Idecl);
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

bool TypeCheck::ensureExportConstraints(AddDecl *add)
{
    Domoid *domoid = add->getImplementedDomoid();
    IdentifierInfo *domainName = domoid->getIdInfo();
    Location domainLoc = domoid->getLocation();

    bool allOK = true;

    // The domoid contains all of the declarations inherited from the super
    // signatures and any associated with expression.  Traverse the set of
    // declarations and ensure that the AddDecl provides a definition.
    for (Domoid::ConstDeclIter iter = domoid->beginDecls();
         iter != domoid->endDecls(); ++iter) {
        Decl *decl   = *iter;
        Type *target = 0;

        // Extract the associated type from this decl.
        if (SubroutineDecl *routineDecl = dyn_cast<SubroutineDecl>(decl))
            target = routineDecl->getType();
        else if (ValueDecl *valueDecl = dyn_cast<ValueDecl>(decl))
            target = valueDecl->getType();

        // FIXME: We need a better diagnostic here.  In particular, we should be
        // reporting which signature(s) demand the missing export.  However, the
        // current organization makes this difficult.  One solution is to link
        // declaration nodes with those provided by the original signature
        // definition.  Another is to perform a search thru the signature
        // hierarchy using TypeCheck::findDecl.
        if (target) {
            Decl *candidate = add->findDecl(decl->getIdInfo(), target);
            SubroutineDecl *srDecl = dyn_cast_or_null<SubroutineDecl>(candidate);
            if (!candidate || (srDecl && !srDecl->hasBody())) {
                report(domainLoc, diag::MISSING_EXPORT)
                    << domainName << decl->getIdInfo();
                allOK = false;
            }
        }
    }
    return allOK;
}

void TypeCheck::importDeclRegion(DeclRegion *region)
{
    // FIXME: We should be able to import a region directly into a scope, thus
    // making these declarations indirect.  However, we do not have appropriate
    // scope API's yet.
    typedef DeclRegion::DeclIter iterator;

    for (iterator I = region->beginDecls(); I != region->endDecls(); ++I)
        scope->addDirectDecl(*I);
}

/// Returns true if the given parameter is of mode "in", and thus capatable with
/// a function declaration.  Otherwise false is returned an a diagnostic is
/// posted.
bool TypeCheck::checkFunctionParameter(ParamValueDecl *param)
{
    PM::ParameterMode mode = param->getParameterMode();
    if (mode == PM::MODE_IN)
        return true;
    report(param->getLocation(), diag::OUT_MODE_IN_FUNCTION);
    return false;
}

/// Returns true if the given descriptor does not contain any duplicate formal
/// parameters.  Otherwise false is returned and the appropriate diagnostic is
/// posted.
bool TypeCheck::checkDescriptorDuplicateParams(Descriptor &desc)
{
    typedef Descriptor::paramIterator iterator;

    bool status = true;
    iterator I = desc.beginParams();
    iterator E = desc.endParams();
    while (I != E) {
        Decl *p1 = cast_node<Decl>(*I);
        for (iterator J = ++I; J != E; ++J) {
            Decl *p2 = cast_node<Decl>(*J);
            if (p1->getIdInfo() == p2->getIdInfo()) {
                report(p2->getLocation(), diag::DUPLICATE_FORMAL_PARAM)
                    << p2->getString();
                status = false;
            }
        }
    }
    return status;
}

/// Returns true if the descriptor does not contain any parameters with the
/// given name.  Otherwise false is returned and the appropriate diagnostic is
/// posted.
bool TypeCheck::checkDescriptorDuplicateParams(Descriptor &desc,
                                               IdentifierInfo *idInfo,
                                               Location loc)
{
    typedef Descriptor::paramIterator iterator;

    iterator I = desc.beginParams();
    iterator E = desc.endParams();
    for ( ; I != E; ++I) {
        Decl *param = cast_node<Decl>(*I);
        if (param->getIdInfo() == idInfo) {
            report(loc, diag::DUPLICATE_FORMAL_PARAM) << idInfo;
            return false;
        }
    }
    return true;
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
        return (std::strncmp(name, "<=", 2) or
                std::strncmp(name, ">=", 2));
}
