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

void TypeCheck::beginModelDeclaration(Descriptor &desc)
{
    assert((desc.isSignatureDescriptor() || desc.isDomainDescriptor()) &&
           "Beginning a mode with is neither a signature or domain?");
    pushModelScope();
}

void TypeCheck::endModelDefinition()
{
    ModelDecl *result = getCurrentModel();
    popScope();
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
        declarativeRegion->addDecl(dom);
        addModel(dom);
        return Node(dom);
    }
    else {
        report(loc, diag::NOT_A_SIGNATURE) << formal->getString();
        getCurrentModel()->markInvalid();
        return Node::getInvalidNode();
    }
}

void TypeCheck::acceptModelDeclaration(Descriptor &desc)
{
    llvm::SmallVector<DomainType*, 4> domains;

    // Convert each parameter node into an AbstractDomainDecl.
    for (Descriptor::paramIterator iter = desc.beginParams();
         iter != desc.endParams(); ++iter) {
        AbstractDomainDecl *domain = lift<AbstractDomainDecl>(*iter);
        assert(domain && "Bad Node kind!");
        domains.push_back(domain->getType());
    }

    // Create the appropriate type of model.
    ModelDecl      *modelDecl;
    IdentifierPool &idPool  = resource.getIdentifierPool();
    IdentifierInfo *percent = &idPool.getIdentifierInfo("%");
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
                                      IdentifierInfo **keywords,
                                      Location        *keywordLocs,
                                      unsigned         numKeywords,
                                      Location         loc)
{
    assert(numKeywords <= numArgs && "More keywords than arguments!");

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

    unsigned numPositional = numArgs - numKeywords;
    llvm::SmallVector<DomainType*, 4> arguments(numArgs);

    // First, populate the argument vector with any positional parameters.
    for (unsigned i = 0; i < numPositional; ++i)
        arguments[i] = lift<DomainType>(argumentNodes[i]);

    // Process any keywords provided.
    for (unsigned i = 0; i < numKeywords; ++i) {
        IdentifierInfo *keyword    = keywords[i];
        Location        keywordLoc = keywordLocs[i];
        int             keywordIdx = candidate->getKeywordIndex(keyword);

        // Ensure the given keyword exists.
        if (keywordIdx < 0) {
            report(keywordLoc, diag::TYPE_HAS_NO_SUCH_KEYWORD)
                << keyword->getString() << candidate->getString();
                return Node::getInvalidNode();
        }

        // The corresponding index of the keyword must be greater than the
        // number of supplied positional parameters (otherwise it would
        // `overlap' a positional parameter).
        if ((unsigned)keywordIdx < numPositional) {
            report(keywordLoc, diag::PARAM_PROVIDED_POSITIONALLY)
                << keyword->getString();
            return Node::getInvalidNode();
        }

        // Ensure that this keyword is not a duplicate of any preceeding
        // keyword.
        for (unsigned j = 0; j < i; ++j) {
            if (keywords[j] == keyword) {
                report(keywordLoc, diag::DUPLICATE_KEYWORD)
                    << keyword->getString();
                return Node::getInvalidNode();
            }
        }

        // Lift the argument node and add it to the set of arguments in its
        // proper position.
        DomainType *argument =
            lift<DomainType>(argumentNodes[i + numPositional]);
        arguments[keywordIdx] = argument;
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
    // ambiguity that can arise is wrt conflicting argument keyword sets.
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
SubroutineDecl *findDecl(const AstRewriter &rewrites,
                         Sigoid            *sig,
                         SubroutineDecl    *decl)
{
    IdentifierInfo *name       = decl->getIdInfo();
    SubroutineType *targetType = decl->getType();

    Sigoid::DeclRange range = sig->findDecls(name);

    for (Sigoid::DeclIter iter = range.first; iter != range.second; ++iter) {
        if (SubroutineDecl *source = dyn_cast<SubroutineDecl>(iter->second)) {
            SubroutineType *sourceType = source->getType();
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
    // same name and type but have disjoint keyword sets) we remember which
    // (non-direct) declarations in sig need an explicit redeclaration using the
    // following SmallPtrSet.  Once all the declarations are processed, we
    // iterate over the set and remove any declarations found to be in conflict.
    typedef llvm::SmallPtrSet<SubroutineDecl*, 4> BadDeclSet;
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
            if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(iter->second)) {
                IdentifierInfo *name   = srDecl->getIdInfo();
                SubroutineType *srType = srDecl->getType();
                SubroutineDecl *decl   = findDecl(rewrites, sig, srDecl);

                // If a matching declaration was not found, apply the rewrites
                // and construct a new indirect declaration node for this
                // signature.
                if (!decl) {
                    SubroutineType *rewriteType = rewrites.rewrite(srType);
                    SubroutineDecl *rewriteDecl;

                    rewriteDecl = makeSubroutineDecl(name, 0, rewriteType, sig);
                    rewriteDecl->setBaseDeclaration(srDecl);
                    rewriteDecl->setDeclarativeRegion(sig);
                    sig->addDecl(rewriteDecl);
                } else if (decl->getBaseDeclaration()) {
                    // A declaration was found which has a base declaration
                    // (meaning that it was not directly declared in this
                    // signature, but inherited from a super).  Since there is
                    // no overridding declaration in this case ensure that the
                    // keywords match.
                    SubroutineType *declType = decl->getType();

                    if (!declType->keywordsMatch(srType)) {
                        Location          sigLoc = sig->getLocation();
                        SubroutineDecl *baseDecl = decl->getBaseDeclaration();
                        SourceLocation     sloc1 =
                            getSourceLocation(baseDecl->getLocation());
                        SourceLocation     sloc2 =
                            getSourceLocation(srDecl->getLocation());
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
            SubroutineDecl *badDecl = *iter;
            sig->removeDecl(badDecl);
            delete badDecl;
        }
    }
}

Node TypeCheck::acceptDeclaration(IdentifierInfo *name,
                                  Node            typeNode,
                                  Location        loc)
{
    Type *type = lift<Type>(typeNode);

    assert(type && "Bad node kind!");
    assert(false && "Declaration type not yet supported!");
    return Node::getInvalidNode();
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
    // location of the type.  The type node here should be a DeclRef or similar
    // which encapsulates the needed location information.
    DomainType *dom = ensureDomainType(typeNode, loc);

    if (!dom) return Node::getInvalidNode();

    // Create a declaration node for this parameter.
    ParamValueDecl *paramDecl = new ParamValueDecl(formal, loc, dom, mode);

    return Node(paramDecl);
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

            ParamValueDecl *param = lift<ParamValueDecl>(*iter);
            assert(param && "Function descriptor with invalid parameter decl!");

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
        // If this descriptor is a function, then we must have a return type
        // that resolves to a domain.
        DomainType *returnType = ensureDomainType(desc.getReturnType(), 0);
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

    if (!routineDecl) return Node::getInvalidNode();

    // Check that this declaration does not conflict with any other.
    if (Decl *extantDecl = region->findDecl(name, routineDecl->getType())) {
        SourceLocation sloc = getSourceLocation(extantDecl->getLocation());
        report(location, diag::SUBROUTINE_REDECLARATION)
            << routineDecl->getString()
            << sloc;
        return Node::getInvalidNode();
    }

    // Add the declaration into the current declarative region.
    region->addDecl(routineDecl);
    return Node(routineDecl);
}


void TypeCheck::endFunctionDefinition()
{
    declarativeRegion = declarativeRegion->getParent();
    popScope();
}
