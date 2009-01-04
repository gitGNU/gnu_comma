//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

// FIXME:  Provide access to the global IdentifierPool.  Remove once this
// resource has a better interface.
#include "comma/basic/IdentifierPool.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Decl.h"
#include <cstdarg>
#include <cstdio>

using namespace comma;
using llvm::dyn_cast;


TypeCheck::TypeCheck(Diagnostic &diag,
                     TextProvider &tp,
                     CompilationUnit *cunit)
    : diagnostic(diag),
      txtProvider(tp),
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

void TypeCheck::beginModelDefinition(DefinitionKind kind,
                                     IdentifierInfo *id, Location loc)
{
    ModelDecl *model;

    switch (kind) {
    case Signature:
        model = new SignatureDecl(id, loc);
        break;

    case Domain:
        model = new DomainDecl(id, loc);
        break;

    case Variety:
        model = new VarietyDecl(id, loc);
        break;

    case Functor:
        model = new FunctorDecl(id, loc);
        break;

    default:
        assert(false && "Bad DefinitionKind!");
    }
    pushModelScope(model);
}

void TypeCheck::endModelDefinition()
{
    ModelDecl *result = currentModel;
    popModelScope();
    addModel(result);
}

void TypeCheck::acceptModelParameter(IdentifierInfo *formal,
                                     Node typeNode, Location loc)
{
    ModelType *type = lift<ModelType>(typeNode);
    const char *name = formal->getString();
    ParameterizedModel *model = currentModel->getParameterizedModel();

    assert(currentScopeKind() == Scope::MODEL_SCOPE);
    assert(model && "Current model is not parameterized!");
    assert(type && "Bad node kind!");

    if (model->getFormalParams()->isDuplicateFormal(formal)) {
        report(loc, diag::DUPLICATE_FORMAL_PARAM) << name;
        currentModel->markInvalid();
    }

    // Check that the parameter type denotes a signature.
    if (SignatureType *sig = llvm::dyn_cast<SignatureType>(type)) {
        // Create a domain to represent the formal parameter.
        DomainDecl *decl = new DomainDecl(formal, loc, sig);

        // Register the formal with the current model (the model takes ownership
        // of the associated decl).
        model->addParameter(formal, sig, decl->getCorrespondingType());

        // Bring the parameter into scope.
        addModel(decl);
    }
    else {
        report(loc, diag::NOT_A_SIGNATURE) << name;
        currentModel->markInvalid();
    }
}

void TypeCheck::beginModelSupersignatures()
{
}

void TypeCheck::acceptModelSupersignature(Node typeNode, const Location loc)
{
    ModelType *type = lift<ModelType>(typeNode);
    SignatureType *sig = llvm::dyn_cast<SignatureType>(type);

    assert(type && "Bad node kind!");

    if (!sig) {
        const char *name = type->getDeclaration()->getString();
        report(loc, diag::NOT_A_SIGNATURE) << name;
        delete type;
        return;
    }

    Sigoid *target;
    if ( !(target = llvm::dyn_cast<Sigoid>(currentModel))) {
        DomainDecl *dom = llvm::dyn_cast<DomainDecl>(currentModel);
        target = dom->getPrincipleSignature()->getDeclaration();
    }
    target->addSupersignature(sig);
}

void TypeCheck::endModelSupersignatures()
{
    currentModel->setTypeComplete();
    ModelDecl::assumption_iterator iter;
    ModelDecl::assumption_iterator endIter = currentModel->endAssumptions();
    PercentType *percent = currentModel->getPercent();
    for (iter = currentModel->beginAssumptions(); iter != endIter; ++iter) {
        SignatureType *assumption = iter->type;
        Location loc = iter->loc;
        if (!percent->has(assumption)) {
            report(loc, diag::DOES_NOT_SATISFY)
                << percent->getString() << assumption->getString();
        }
    }
}

Node TypeCheck::acceptTypeIdentifier(IdentifierInfo *id, Location loc)
{
    ModelDecl *model = lookupModel(id);
    const char *name = id->getString();

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node();
    }
    else if (SignatureDecl *sig = llvm::dyn_cast<SignatureDecl>(model)) {
        return Node(sig->getCorrespondingType());
    }
    else if (DomainDecl *dom = llvm::dyn_cast<DomainDecl>(model)) {
        return Node(dom->getCorrespondingType());
    }
    else {
        // Otherwise, we have a variety or functor decl.
        //
        // FIXME:  This diagnostic needs improvment.
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return Node();
    }
}

Node TypeCheck::acceptTypeApplication(IdentifierInfo *connective,
                                      Node *argumentNodes, unsigned numArgs,
                                      Location loc)
{
    ModelDecl *model = lookupModel(connective);
    const char *name = connective->getString();
    bool isValid     = false;
    unsigned arity;

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node();
    }
    else {
        ParameterizedModel *pmodel = model->getParameterizedModel();
        if (!pmodel) {
            report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
            return Node();
        }

        arity = pmodel->getArity();
        if (arity != numArgs) {
            report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << model->getString();
            return Node();
        }

        llvm::SmallVector<ModelType*, 4> arguments;
        ParameterList *params = pmodel->getFormalParams();
        for (unsigned i = 0; i < arity; ++i) {
            ModelType *argument = lift<ModelType>(argumentNodes[i]);
            SignatureType *target = dyn_cast<SignatureType>(params->getType(i));

            assert(argument && "Bad node kind!");
            assert(target && "Model parameter not a domain!");

            if (!argument->denotesDomainType()) {
                // FIXME: A better diagnostic is needed.  In particular, this
                // function should take a vector of locations for each argument.
                report(loc, diag::NOT_A_DOMAIN) << argument->getString();
                return Node();
            }

            if (!argument->has(target)) {
                // If the argument type is %, and the corresponding model is not
                // type complete, add an assumption which we will verify later.
                if (PercentType *percent = dyn_cast<PercentType>(argument)) {
                    assert(percent->getDeclaration() == currentModel);
                    if (!currentModel->isTypeComplete()) {
                        currentModel->addAssumption(target, loc);
                        arguments.push_back(argument);
                        continue;
                    }
                }
                report(loc, diag::DOES_NOT_SATISFY)
                    << argument->getString() << target->getString();
                return Node();
            }
            arguments.push_back(argument);
        }

        // Obtain a memoized type node for this model and argument set.
        Node node(pmodel->getCorrespondingType(&arguments[0], arity));
        return node;
    }
}

Node TypeCheck::acceptPercent(Location loc)
{
    return Node(currentModel->getPercent());
}


//===----------------------------------------------------------------------===//
// Utility Functions.
//===----------------------------------------------------------------------===//

void TypeCheck::deleteNode(Node node)
{
    delete Node::lift<Ast>(node);
}

bool TypeCheck::checkDuplicateFormalParameters(
    const std::vector<IdentifierInfo *> &formals,
    const std::vector<Location> &formalLocations)
{
    bool duplicatesDetected = false;
    for (unsigned i = 0; i < formals.size() - 1; ++i) {
        IdentifierInfo *x = formals[i];
        for (unsigned j = i + 1; j < formals.size(); ++j) {
            IdentifierInfo *y = formals[j];
            if (x == y) {
                Location loc = formalLocations[j];
                const char *name  = y->getString();
                report(loc, diag::DUPLICATE_FORMAL_PARAM) << name;
                duplicatesDetected = true;
            }
        }
    }
    return duplicatesDetected;
}
