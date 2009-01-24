//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Decl.h"
#include "TypeEqual.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::isa;

TypeCheck::TypeCheck(Diagnostic      &diag,
                     AstResource     &resource,
                     CompilationUnit *cunit)
    : diagnostic(diag),
      resource(resource),
      compUnit(cunit),
      errorCount(0)
{
    topScope = new Scope();
    currentScope = topScope;
}

TypeCheck::~TypeCheck()
{
    delete topScope;
}

void TypeCheck::deleteNode(Node node)
{
    Ast *ast = Node::lift<Ast>(node);
    if (ast && ast->isDeletable()) delete ast;
}

void TypeCheck::beginSignatureDefinition(IdentifierInfo *name,
                                         Location        loc)
{
    currentModelInfo = new ModelInfo(Ast::AST_SignatureDecl, name, loc);
    pushModelScope();
}

void TypeCheck::beginDomainDefinition(IdentifierInfo *name,
                                      Location        loc)
{
    currentModelInfo = new ModelInfo(Ast::AST_DomainDecl, name, loc);
    pushModelScope();
}

void TypeCheck::endModelDefinition()
{
    ModelDecl *result = getCurrentModel();
    popModelScope();
    addModel(result);
}

Node TypeCheck::acceptModelParameter(IdentifierInfo *formal,
                                     Node            typeNode,
                                     Location        loc)
{
    ModelType *type = lift<ModelType>(typeNode);

    assert(currentScopeKind() == Scope::MODEL_SCOPE);
    assert(type && "Bad node kind!");

    // Check that the parameter type denotes a signature.  For each parameter,
    // we create an AbstractDomainType to represent the formal, and add that
    // type into the current scope so that it may participate in upcomming
    // parameter types.
    if (SignatureType *sig = dyn_cast<SignatureType>(type)) {
        // If the current scope contains a type declaration of the same name, it
        // must be a formal parameter (since parameters are the first items
        // brought into scope, and we explicitly request that parent scopes are
        // not traversed).
        if (lookupModel(formal, false)) {
            report(loc, diag::DUPLICATE_FORMAL_PARAM) << formal->getString();
            return Node::getInvalidNode();
        }
        AbstractDomainDecl *dom = new AbstractDomainDecl(formal, sig, loc);
        addModel(dom);
        return Node(dom);
    }
    else {
        report(loc, diag::NOT_A_SIGNATURE) << formal->getString();
        getCurrentModel()->markInvalid();
        return Node::getInvalidNode();
    }
}

// We use this method
void TypeCheck::acceptModelParameterList(Node     *params,
                                         Location *paramLocs,
                                         unsigned  arity)
{
    llvm::SmallVector<DomainType*, 4> domains;

    // Convert each node to an AbstractDomainDecl.
    for (unsigned i = 0; i < arity; ++i) {
        AbstractDomainDecl *domain = lift<AbstractDomainDecl>(params[i]);
        assert(domain && "Bad Node kind!");
        domains.push_back(domain->getType());
    }

    // Create the appropriate type of model.
    assert(currentModelInfo->decl == 0);
    ModelDecl      *modelDecl;
    IdentifierPool &idPool  = resource.getIdentifierPool();
    IdentifierInfo *percent = &idPool.getIdentifierInfo("%");
    IdentifierInfo *name    = currentModelInfo->name;
    Location        loc     = currentModelInfo->location;
    if (domains.empty()) {
        switch (currentModelInfo->kind) {

        case Ast::AST_SignatureDecl:
            modelDecl = new SignatureDecl(percent, name, loc);
            break;

        case Ast::AST_DomainDecl:
            modelDecl = new DomainDecl(percent, name, loc);
            break;

        default:
            assert(false && "Corruption of currentModelInfo->kind!");
        }
    }
    else {
        DomainType **formals = &domains[0];
        switch (currentModelInfo->kind) {

        case Ast::AST_SignatureDecl:
            currentModelInfo->kind = Ast::AST_VarietyDecl;
            modelDecl = new VarietyDecl(percent, name, loc, formals, arity);
            break;

        case Ast::AST_DomainDecl:
            currentModelInfo->kind = Ast::AST_FunctorDecl;
            modelDecl = new FunctorDecl(percent, name, loc, formals, arity);
            break;

        default:
            assert(false && "Corruption of currentModelInfo->kind!");
        }
    }

    declarativeRegion = modelDecl->asDeclarativeRegion();
    currentModelInfo->decl = modelDecl;
}

Node TypeCheck::acceptWithSupersignature(Node     typeNode,
                                         Location loc)
{
    ModelType     *type = lift<ModelType>(typeNode);
    SignatureType *superSig;
    Sigoid        *currentSig;

    assert(type && "Bad node kind!");

    // Simply check that the node denotes a signature.
    superSig = dyn_cast<SignatureType>(type);
    if (!superSig) {
        const char *name = type->getString();
        report(loc, diag::NOT_A_SIGNATURE) << name;
        delete type;
        return Node::getInvalidNode();
    }

    // Register the signature.
    currentSig = getCurrentSignature();
    currentSig->addSupersignature(superSig);

    return typeNode;
}

Node TypeCheck::acceptPercent(Location loc)
{
    ModelDecl *model = getCurrentModel();

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << "%";
        return Node::getInvalidNode();
    }

    return Node(model->getPercent());
}

Node TypeCheck::acceptTypeIdentifier(IdentifierInfo *id,
                                     Location        loc)
{
    ModelDecl  *model = lookupModel(id);
    const char *name  = id->getString();

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node::getInvalidNode();
    }
    else if (isa<SignatureDecl>(model)
             || isa<DomainDecl>(model)
             || isa<AbstractDomainDecl>(model))
        return Node(model->getType());
    else {
        // Otherwise, we have a variety or functor decl.
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return Node::getInvalidNode();
    }
}

Node TypeCheck::acceptTypeApplication(IdentifierInfo  *connective,
                                      Node            *argumentNodes,
                                      Location        *argumentLocs,
                                      unsigned         numArgs,
                                      IdentifierInfo **selectors,
                                      Location        *selectorLocs,
                                      unsigned         numSelectors,
                                      Location         loc)
{
    assert(numSelectors <= numArgs && "More selectors than arguments!");

    ModelDecl  *model = lookupModel(connective);
    const char *name  = connective->getString();

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node();
    }

    ParameterizedType *candidate =
        dyn_cast<ParameterizedType>(model->getType());

    if (!candidate) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return Node::getInvalidNode();
    }

    if (candidate->getArity() != numArgs) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return Node::getInvalidNode();
    }

    unsigned numPositional = numArgs - numSelectors;
    llvm::SmallVector<DomainType*, 4> arguments(numArgs);

    // First, populate the argument vector with any positional parameters.
    for (unsigned i = 0; i < numPositional; ++i)
        arguments[i] = lift<DomainType>(argumentNodes[i]);

    // Process any selectors provided.
    for (unsigned i = 0; i < numSelectors; ++i) {
        IdentifierInfo *selector    = selectors[i];
        Location        selectorLoc = selectorLocs[i];
        int             selectorIdx = candidate->getSelectorIndex(selector);

        // Ensure the given selector exists.
        if (selectorIdx < 0) {
            report(selectorLoc, diag::TYPE_HAS_NO_SUCH_SELECTOR)
                << selector->getString() << candidate->getString();
                return Node::getInvalidNode();
        }

        // The corresponding index of the selector must be greater than the
        // number of supplied positional parameters (otherwise it would
        // `overlap' a positional parameter).
        if ((unsigned)selectorIdx < numPositional) {
            report(selectorLoc, diag::SELECTED_PARAM_PROVIDED_POSITIONALLY)
                << selector->getString();
            return Node::getInvalidNode();
        }

        // Ensure that this selector is not a duplicate of any preceeding
        // selector.
        for (unsigned j = 0; j < i; ++j) {
            if (selectors[j] == selector) {
                report(selectorLoc, diag::DUPLICATE_SELECTOR)
                    << selector->getString();
                return Node::getInvalidNode();
            }
        }

        // Lift the argument node and add it to the set of arguments in its
        // proper position.
        DomainType *argument =
            lift<DomainType>(argumentNodes[i + numPositional]);
        arguments[selectorIdx] = argument;
    }

    // Check each argument type.
    for (unsigned i = 0; i < numArgs; ++i) {
        DomainType  *argument = arguments[i];
        Location       argLoc = argumentLocs[i];
        SignatureType *target =
                resolveArgumentType(candidate, &arguments[0], i);

        if (!argument) {
                ModelType *model = lift<ModelType>(argumentNodes[i]);
                report(argLoc, diag::NOT_A_DOMAIN) << model->getString();
                return Node::getInvalidNode();
        }

        if (!has(argument, target)) {
            report(argLoc, diag::DOES_NOT_SATISFY)
                << argument->getString() << target->getString();
            return Node::getInvalidNode();
        }
    }

    // Obtain a memoized type node for this particular argument set.
    Node node;
    if (VarietyDecl *variety = dyn_cast<VarietyDecl>(model)) {
        node = Node(variety->getCorrespondingType(&arguments[0], numArgs));
    }
    else {
        FunctorDecl *functor = dyn_cast<FunctorDecl>(model);
        node = Node(functor->getCorrespondingType(&arguments[0], numArgs));
    }
    return node;
}

Node TypeCheck::acceptFunctionType(IdentifierInfo **formals,
                                   Location        *formalLocations,
                                   Node            *types,
                                   Location        *typeLocations,
                                   unsigned         arity,
                                   Node             returnType,
                                   Location         returnLocation)
{
    llvm::SmallVector<DomainType*, 4> argumentTypes;
    DomainType *targetType;
    bool allOK = true;

    for (unsigned i = 0; i < arity; ++i) {
        IdentifierInfo *formal    = formals[i];
        Location        formalLoc = formalLocations[i];

        // Check the current formal against all preceeding parameters and report
        // any duplication.
        for (unsigned j = 0; j < i; ++j)
            if (formal == formals[j]) {
                allOK = false;
                report(formalLoc, diag::DUPLICATE_FORMAL_PARAM)
                    << formal->getString();
            }

        // Ensure each parameter denotes a domain type.
        DomainType *argumentType = ensureDomainType(types[i], typeLocations[i]);
        if (argumentType && allOK)
            argumentTypes.push_back(argumentType);
    }

    // Ensure the return type denotes a domain type.
    targetType = ensureDomainType(returnType, returnLocation);

    if (targetType && allOK) {
        FunctionType *ftype =
            new FunctionType(&formals[0], &argumentTypes[0], arity, targetType);
        return Node(ftype);
    }
    return Node::getInvalidNode();
}

void TypeCheck::beginWithExpression()
{
    // If the current model is a signature, we set the current declarative
    // region to the signature itself.  If we are processing a domain, set the
    // declarative region to the principle signature of the domain.
    ModelDecl *currentModel = getCurrentModel();

    if (Domoid *domain = dyn_cast<Domoid>(currentModel))
        declarativeRegion = domain->getPrincipleSignature();
    else {
        Sigoid *signature = dyn_cast<Sigoid>(currentModel);
        declarativeRegion = signature;
        assert(signature && "current model is neither a domain or signature!");
    }
}

void TypeCheck::endWithExpression()
{
    // Ensure that all ambiguous declarations are redeclared.  For now, the only
    // ambiguity that can arise is wrt conflicting argument selector sets.
    ensureNecessaryRedeclarations(getCurrentSignature());
}

// The following function resolves the argument type of a functor or variety
// given previous actual arguments.  That is, for a dependent argument list of
// the form (X : T, Y : U(X)), this function resolves the type of U(X) given an
// actual parameter for X.
SignatureType *TypeCheck::resolveArgumentType(ParameterizedType *type,
                                              DomainType       **actuals,
                                              unsigned           numActuals)
{
    AstRewriter rewriter;

    // For each actual argument, establish a map from the formal parameter to
    // the actual.
    for (unsigned i = 0; i < numActuals; ++i) {
        DomainType *formal = type->getFormalDomain(i);
        DomainType *actual = actuals[i];
        rewriter.addRewrite(formal, actual);
    }

    SignatureType *target = type->getFormalType(numActuals);
    return rewriter.rewrite(target);
}

Sigoid *TypeCheck::getCurrentSignature() const {
    Sigoid *result = dyn_cast<Sigoid>(getCurrentModel());
    if (result == 0) {
        Domoid *dom = dyn_cast<Domoid>(getCurrentModel());
        result = dom->getPrincipleSignature();
    }
    return result;
}

Domoid *TypeCheck::getCurrentDomain() const {
    return dyn_cast<Domoid>(getCurrentModel());
}

DomainType *TypeCheck::ensureDomainType(Node     node,
                                        Location loc) const
{
    DomainType *dom = lift<DomainType>(node);
    if (!dom) {
        ModelType *model = lift<ModelType>(node);
        assert(model && "Bad node kind!");
        report(loc, diag::NOT_A_DOMAIN) << model->getString();
        return 0;
    }
    return dom;
}

namespace {

// This function is a helper for ensureNecessaryRedeclarations.  It searches all
// declarations present in the given sigoid for a direct or indirect decl for a
// match with respect to the given rewrites.  If a matching declaration is found
// the matching node is returned, else NULL.
FunctionDecl *findDecl(const AstRewriter &rewrites,
                       Sigoid            *sig,
                       FunctionDecl      *decl)
{
    IdentifierInfo *name       = decl->getIdInfo();
    FunctionType   *targetType = decl->getType();

    Sigoid::DeclRange range = sig->findDecls(name);

    for (Sigoid::DeclIter iter = range.first; iter != range.second; ++iter) {
        if (FunctionDecl *source = dyn_cast<FunctionDecl>(iter->second)) {
            FunctionType *sourceType = source->getType();
            if (compareTypesUsingRewrites(rewrites, sourceType, targetType))
                return source;
        }
    }
    return 0;
}

} // End anonymous namespace.

void TypeCheck::ensureNecessaryRedeclarations(Sigoid *sig)
{
    // We scan the set of declarations for each direct super signature of sig.
    // When a declaration is found which is not already declared in sig, we add
    // it on good faith that all upcoming declarations will not conflict.
    //
    // When a conflict occurs (that is, when two declarations exists with the
    // same name and type but have disjoint selector sets) we remember which
    // (non-direct) declarations in sig need an explicit redeclaration using the
    // following SmallPtrSet.  Once all the declarations are processed, we
    // iterate over the set and remove any declarations found to be in conflict.
    typedef llvm::SmallPtrSet<FunctionDecl*, 4> BadDeclSet;
    BadDeclSet badDecls;

    Sigoid::sig_iterator superIter    = sig->beginDirectSupers();
    Sigoid::sig_iterator endSuperIter = sig->endDirectSupers();
    for ( ; superIter != endSuperIter; ++superIter) {
        SignatureType *super   = *superIter;
        Sigoid        *sigdecl = super->getDeclaration();
        AstRewriter    rewrites;

        rewrites[sigdecl->getPercent()] = sig->getPercent();
        rewrites.installRewrites(super);

        Sigoid::DeclIter iter    = sigdecl->beginDecls();
        Sigoid::DeclIter endIter = sigdecl->endDecls();
        for ( ; iter != endIter; ++iter) {
            if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(iter->second)) {
                IdentifierInfo *name  = fdecl->getIdInfo();
                FunctionType   *ftype = fdecl->getType();
                FunctionDecl   *decl  = findDecl(rewrites, sig, fdecl);

                // If a matching declaration was not found, apply the rewrites
                // and construct a new indirect declaration node for this
                // signature.
                if (!decl) {
                    FunctionType *rewriteType = rewrites.rewrite(ftype);
                    FunctionDecl *rewriteDecl =
                        new FunctionDecl(name, rewriteType, 0);
                    rewriteDecl->setBaseDeclaration(fdecl);
                    rewriteDecl->setDeclarativeRegion(sig);
                    sig->addDecl(rewriteDecl);
                } else if (decl->getBaseDeclaration()) {
                    // A declaration was found which has a base declaration
                    // (meaning that it was not directly declared in this
                    // signature, but inherited from a super).  Since there is
                    // no overridding declaration in this case ensure that the
                    // selectors match.
                    FunctionType *declType = decl->getType();

                    if (!declType->selectorsMatch(ftype)) {
                        Location        sigLoc = sig->getLocation();
                        FunctionDecl *baseDecl = decl->getBaseDeclaration();
                        SourceLocation sloc1 =
                            getSourceLocation(baseDecl->getLocation());
                        SourceLocation sloc2 =
                            getSourceLocation(fdecl->getLocation());
                        report(sigLoc, diag::MISSING_REDECLARATION)
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
             iter != badDecls.end();
             ++iter) {
            FunctionDecl *badDecl = *iter;
            sig->removeDecl(badDecl);
            delete badDecl;
        }
    }
}

void TypeCheck::acceptDeclaration(IdentifierInfo *name,
                                  Node            typeNode,
                                  Location        loc)
{
    DeclarativeRegion *region = currentDeclarativeRegion();
    Type *type = lift<Type>(typeNode);

    assert(type && "Bad node kind!");

    if (FunctionType *ftype = dyn_cast<FunctionType>(type)) {
        if (Decl *extantDecl = region->findDecl(name, type)) {
            SourceLocation sloc = getSourceLocation(extantDecl->getLocation());
            report(loc, diag::FUNCTION_REDECLARATION) << name->getString()
                                                      << sloc;
            return;
        }
        FunctionDecl *fdecl = new FunctionDecl(name, ftype, loc);
        region->addDecl(fdecl);
        fdecl->setDeclarativeRegion(region);
    }
    else {
        assert(false && "Declaration type not yet supported!");
    }
}

void TypeCheck::beginAddExpression()
{
    Domoid *domoid = getCurrentDomain();
    assert(domoid && "Processing `add' expression outside domain context!");

    // Switch to the declarative region which this domains AddDecl provides.
    declarativeRegion = domoid->getImplementation();
    assert(declarativeRegion && "Domain missing Add declaration node!");

    // Enter a new scope for the add expression.
    pushScope();
}

void TypeCheck::endAddExpression()
{
    // Leave the scope corresponding to the add expression and switch back to
    // the declarative region of the defining domain.
    declarativeRegion = declarativeRegion->getParent();
    assert(declarativeRegion == getCurrentModel()->asDeclarativeRegion());
    popScope();
}
