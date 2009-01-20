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
#include <cstdarg>
#include <cstdio>

#include <iostream>

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
    addType(result->getType());
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
        // brought into scope).
        if (lookupType(formal, false)) {
            report(loc, diag::DUPLICATE_FORMAL_PARAM) << formal->getString();
            return Node::getInvalidNode();
        }
        AbstractDomainType *dom = new AbstractDomainType(formal, sig);
        addType(dom);
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
    llvm::SmallVector<AbstractDomainType*, 4> domains;

    // Convert each node to an AbstractDomainType.
    for (unsigned i = 0; i < arity; ++i) {
        AbstractDomainType *domain = lift<AbstractDomainType>(params[i]);
        assert(domain && "Bad Node kind!");
        domains.push_back(domain);
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
        AbstractDomainType **formals = &domains[0];
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
    currentModelInfo->decl = modelDecl;
}

Node TypeCheck::acceptModelSupersignature(Node     typeNode,
                                          Location loc)
{
    ModelType *type = lift<ModelType>(typeNode);
    SignatureType *superSig;
    Sigoid *currentSig;

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

void TypeCheck::acceptModelSupersignatureList(Node    *sigs,
                                              unsigned numSigs)
{
    // Nothing to do in this case.
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
    ModelType  *type = lookupType(id);
    const char *name = id->getString();

    if (type == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node::getInvalidNode();
    }
    else if (isa<SignatureType>(type) || isa<DomainType>(type))
        return Node(type);
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
    assert(numSelectors <= numArgs && "More selectors that arguments!");

    ModelType  *type = lookupType(connective);
    const char *name = connective->getString();

    if (type == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node();
    }

    ParameterizedType *candidate = dyn_cast<ParameterizedType>(type);

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
        // number of positional parameters (otherwise it would `overlap' a
        // positional parameter).
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
    if (VarietyType *variety = dyn_cast<VarietyType>(candidate)) {
        VarietyDecl *decl = variety->getVarietyDecl();
        node = Node(decl->getCorrespondingType(&arguments[0], numArgs));
    }
    else {
        FunctorType *functor = dyn_cast<FunctorType>(candidate);
        assert(functor && "Corrupt hierarchy?");
        FunctorDecl *decl = functor->getFunctorDecl();
        node = Node(decl->getCorrespondingType(&arguments[0], numArgs));
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

Node TypeCheck::acceptSignatureComponent(IdentifierInfo *name,
                                         Node            typeNode,
                                         Location        loc)
{
    Sigoid       *sig   = getCurrentSignature();
    FunctionType *ftype = lift<FunctionType>(typeNode);

    assert(ftype && "Only function decls currently supported!");

    // Ensure that this is not a redeclaration.
    FunctionDecl *extantDecl = sig->findDirectComponent(name, ftype);
    if (extantDecl) {
        report(loc, diag::FUNCTION_REDECLARATION) << name->getString();
        return Node::getInvalidNode();
    }

    PercentType *percent = sig->getPercent();
    FunctionDecl  *fdecl = new FunctionDecl(name, ftype, percent, loc);
    sig->addComponent(fdecl);
    return Node(fdecl);
}

void TypeCheck::acceptSignatureComponentList(Node    *components,
                                             unsigned numComponents)
{
    // Ensure that all ambiguous declarations are redeclared.  For now, the only
    // ambiguity that can arrise is wrt conflicting argument selector sets.
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
        AbstractDomainType *formal = type->getFormalDomain(i);
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

FunctionDecl *findComponent(const AstRewriter &rewrites,
                            Sigoid            *sig,
                            FunctionDecl      *fdecl)
{
    IdentifierInfo *name       = fdecl->getIdInfo();
    FunctionType   *targetType = fdecl->getFunctionType();

    Sigoid::ComponentRange range = sig->findComponents(name);
    Sigoid::ComponentIter   iter = range.first;
    for ( ; iter != range.second; ++iter) {
        FunctionType *sourceType = iter->second->getFunctionType();
        if (compareTypesUsingRewrites(rewrites, sourceType, targetType))
            return iter->second;
    }
    return 0;
}

}

void TypeCheck::ensureNecessaryRedeclarations(Sigoid *sig)
{
    // FIXME:  We should be iterating over the set of direct signatures, not the
    // full signature set.
    Sigoid::sig_iterator superIter    = sig->beginSupers();
    Sigoid::sig_iterator endSuperIter = sig->endSupers();
    for ( ; superIter != endSuperIter; ++superIter) {
        SignatureType *super   = *superIter;
        Sigoid        *sigdecl = super->getDeclaration();
        AstRewriter    rewrites;

        rewrites[sigdecl->getPercent()] = sig->getPercent();
        rewrites.installRewrites(super);

        Sigoid::ComponentIter iter    = sigdecl->beginComponents();
        Sigoid::ComponentIter endIter = sigdecl->endComponents();
        for ( ; iter != endIter; ++iter) {
            IdentifierInfo *name  = iter->first;
            FunctionDecl   *fdecl = iter->second;
            FunctionType   *ftype = fdecl->getFunctionType();

            FunctionDecl *component = findComponent(rewrites, sig, fdecl);

            if (component) {
                FunctionType *componentType = component->getFunctionType();

                // If the component is a direct declaration, then the
                // component overrides the decl originating from the super
                // signature.  Otherwise, the selectors must match.
                if (!component->isTypeContext(sig->getPercent()) &&
                    !componentType->selectorsMatch(ftype)) {
                    report(component->getLocation(),
                           diag::MISSING_REDECLARATION)
                        << component->getString();
                }
            }
            else {
                // Apply the rewrites and construct a new declaration node.
                FunctionType *rewriteType = rewrites.rewrite(ftype);
                FunctionDecl *rewriteDecl =
                    new FunctionDecl(name, rewriteType, super, 0);
                sig->addComponent(rewriteDecl);
            }
        }
    }
}
