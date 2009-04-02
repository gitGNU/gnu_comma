//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Expr.h"
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
      errorCount(0) { }

TypeCheck::~TypeCheck() { }

void TypeCheck::deleteNode(Node node)
{
    Ast *ast = lift_node<Ast>(node);
    if (ast && ast->isDeletable()) delete ast;
}

Sigoid *TypeCheck::getCurrentSignature() const
{
    return dyn_cast<Sigoid>(getCurrentModel());
}

Domoid *TypeCheck::getCurrentDomain() const
{
    return dyn_cast<Domoid>(getCurrentModel());
}

ProcedureDecl *TypeCheck::getCurrentProcedure() const
{
    return dyn_cast<ProcedureDecl>(currentDeclarativeRegion());
}

FunctionDecl *TypeCheck::getCurrentFunction() const
{
    return dyn_cast<FunctionDecl>(currentDeclarativeRegion());
}

void TypeCheck::beginModelDeclaration(Descriptor &desc)
{
    assert((desc.isSignatureDescriptor() || desc.isDomainDescriptor()) &&
           "Beginning a mode with is neither a signature or domain?");
    scope.push(MODEL_SCOPE);
}

void TypeCheck::endModelDefinition()
{
    ModelDecl *result = getCurrentModel();
    scope.pop();
    scope.addDirectModel(result);
}

Node TypeCheck::acceptModelParameter(IdentifierInfo *formal,
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
        // If the current scope contains a type declaration of the same name, it
        // must be a formal parameter (since parameters are the first items
        // brought into scope, and we explicitly request that parent scopes are
        // not traversed).
        if (scope.lookupDirectModel(formal, false)) {
            report(loc, diag::DUPLICATE_FORMAL_PARAM) << formal->getString();
            return Node::getInvalidNode();
        }
        AbstractDomainDecl *dom = new AbstractDomainDecl(formal, sig, loc);
        scope.addDirectModel(dom);
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
        AbstractDomainDecl *domain = cast_node<AbstractDomainDecl>(*iter);
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
    Type          *type = cast_node<Type>(typeNode);
    SignatureType *superSig;

    // Simply check that the node denotes a signature.
    superSig = dyn_cast<SignatureType>(type);
    if (!superSig) {
        report(loc, diag::NOT_A_SIGNATURE);
        delete type;
        return Node::getInvalidNode();
    }

    // Register the signature.
    getCurrentModel()->addDirectSignature(superSig);

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
    TypeDecl   *type = scope.lookupDirectType(id);
    const char *name = id->getString();

    if (type == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node::getInvalidNode();
    }

    switch (type->getKind()) {

    default:
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << name;
        return Node::getInvalidNode();

    case Ast::AST_SignatureDecl:
    case Ast::AST_DomainDecl:
    case Ast::AST_AbstractDomainDecl:
    case Ast::AST_CarrierDecl:
        return Node(type->getType());
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

    ModelDecl  *model = scope.lookupDirectModel(connective);
    const char *name  = connective->getString();

    if (model == 0) {
        report(loc, diag::TYPE_NOT_VISIBLE) << name;
        return Node::getInvalidNode();
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
        Type *argument =
            cast_node<Type>(argumentNodes[i + numPositional]);
        arguments[keywordIdx] = argument;
    }

    // Check each argument type.
    for (unsigned i = 0; i < numArgs; ++i) {
        Type        *argument = arguments[i];
        Location       argLoc = argumentLocs[i];
        SignatureType *target =
                resolveArgumentType(candidate, &arguments[0], i);

        if (DomainType *domain = dyn_cast<DomainType>(argument)) {
            if (!has(domain, target)) {
                report(argLoc, diag::DOES_NOT_SATISFY)
                    << domain->getString()  << target->getString();
                return Node::getInvalidNode();
            }
            continue;
        }

        if (CarrierType *carrier = dyn_cast<CarrierType>(argument)) {
            DomainType *rep =
                dyn_cast<DomainType>(carrier->getRepresentationType());
            if (!rep) {
                report(argLoc, diag::NOT_A_DOMAIN);
                return Node::getInvalidNode();
            }
            if (!has(rep, target)) {
                report(argLoc, diag::DOES_NOT_SATISFY)
                    << carrier->getString() << target->getString();
                return Node::getInvalidNode();
            }
            continue;
        }

        // Otherwise, the argument does not denote a domain, and so cannot
        // satisfy the signature constraint.
        report(argLoc, diag::NOT_A_DOMAIN);
        return Node::getInvalidNode();
    }

    // Obtain a memoized type node for this particular argument set.
    Node node = Node::getInvalidNode();
    if (VarietyDecl *variety = dyn_cast<VarietyDecl>(model)) {
        node = Node(variety->getCorrespondingType(&arguments[0], numArgs));
    }
    else {
        FunctorDecl *functor = cast<FunctorDecl>(model);
        DomainInstanceDecl *instance =
            functor->getInstance(&arguments[0], numArgs);
        node = Node(instance->getType());
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
    llvm::SmallVector<Type*, 4> argumentTypes;
    Type *targetType;
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
        Type *argumentType = ensureDomainType(types[i], typeLocations[i]);
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
    // Nothing to do.  The declarative region of the current model is the
    // destination of all declarations in a with expression.
}

void TypeCheck::endWithExpression()
{
    // Ensure that all ambiguous declarations are redeclared.  For now, the only
    // ambiguity that can arise is wrt conflicting argument keyword sets.
    ensureNecessaryRedeclarations(getCurrentModel());
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

Type *TypeCheck::ensureDomainType(Node     node,
                                  Location loc) const
{
    if (DomainType *dom = lift_node<DomainType>(node))
        return dom;
    else if (CarrierType *carrier = lift_node<CarrierType>(node))
        return carrier;

    report(loc, diag::NOT_A_DOMAIN);
    return 0;
}

namespace {

// This function is a helper for ensureNecessaryRedeclarations.  It searches all
// declarations present in the given declarative region for a match with respect
// to the given rewrites.  Returns a matching delcaration node or null.
SubroutineDecl *findDecl(const AstRewriter &rewrites,
                         DeclarativeRegion *region,
                         SubroutineDecl    *decl)
{
    IdentifierInfo   *name       = decl->getIdInfo();
    SubroutineType   *targetType = decl->getType();
    Sigoid::DeclRange range      = region->findDecls(name);

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
            if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(iter->second)) {
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
                    rewriteDecl->setBaseDeclaration(srDecl);
                    rewriteDecl->setDeclarativeRegion(model);
                    model->addDecl(rewriteDecl);
                } else if (decl->getBaseDeclaration()) {
                    // A declaration was found which has a base declaration
                    // (meaning that it was not directly declared in this
                    // signature, but inherited from a super).  Since there is
                    // no overridding declaration in this case ensure that the
                    // keywords match.
                    SubroutineType *declType = decl->getType();

                    if (!declType->keywordsMatch(srType)) {
                        Location        modelLoc = model->getLocation();
                        SubroutineDecl *baseDecl = decl->getBaseDeclaration();
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
             iter != badDecls.end();
             ++iter) {
            SubroutineDecl *badDecl = *iter;
            model->removeDecl(badDecl);
            delete badDecl;
        }
    }
}

Node TypeCheck::acceptDeclaration(IdentifierInfo *name,
                                  Node            typeNode,
                                  Location        loc)
{
    Type *type = cast_node<Type>(typeNode);

    if (Type *domain = ensureDomainType(type, loc)) {
        ObjectDecl *decl = new ObjectDecl(name, domain, loc);

        // FIXME: Adding the decl now will expose the binding in any
        // initialization expression.  Perhaps we should combine this function
        // and acceprDeclarationInitializer.
        scope.addDirectValue(decl);
        return Node(decl);
    }
    return Node::getInvalidNode();
}

void TypeCheck::acceptDeclarationInitializer(Node declNode, Node initializer)
{
    ObjectDecl *decl       = cast_node<ObjectDecl>(declNode);
    Expr       *expr       = cast_node<Expr>(initializer);
    Type       *targetType = decl->getType();

    if (ensureExprType(expr, targetType))
        decl->setInitializer(expr);
}

Node TypeCheck::acceptImportDeclaration(Node importedNode, Location loc)
{
    Type         *type = cast_node<Type>(importedNode);
    DomainType   *domain;

    if (CarrierType *carrier = dyn_cast<CarrierType>(type))
        domain = dyn_cast<DomainType>(carrier->getRepresentationType());
    else
        domain = dyn_cast<DomainType>(type);

    if (!domain) {
        report(loc, diag::IMPORT_FROM_NON_DOMAIN);
        return Node::getInvalidNode();
    }

    scope.addImport(domain);
    return new ImportDecl(domain, loc);
}


void TypeCheck::beginAddExpression()
{
    Domoid *domoid = getCurrentDomain();
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

    if (Type *type = ensureDomainType(typeNode, loc)) {
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
    Type *dom = ensureDomainType(typeNode, loc);

    if (!dom) return Node::getInvalidNode();

    // Create a declaration node for this parameter.
    ParamValueDecl *paramDecl = new ParamValueDecl(formal, dom, mode, loc);

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
        // If this descriptor is a function, then we must have a return type
        // that resolves to a domain.
        Type *returnType = ensureDomainType(desc.getReturnType(), 0);
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

    // Add the subroutine declaration into the current declarative region.
    region->addDecl(routineDecl);
    scope.addDirectSubroutine(routineDecl);
    return Node(routineDecl);
}

void TypeCheck::beginSubroutineDefinition(Node declarationNode)
{
    SubroutineDecl *srDecl = cast_node<SubroutineDecl>(declarationNode);

    // Enter a scope for the subroutine definition and populate with the formal
    // parmeter bindings.  Set the current declarative region to be that of the
    // subroutine.
    scope.push(FUNCTION_SCOPE);
    declarativeRegion = srDecl->asDeclarativeRegion();

    typedef SubroutineDecl::ParamDeclIterator ParamIter;
    for (ParamIter iter = srDecl->beginParams();
         iter != srDecl->endParams(); ++iter) {
        ParamValueDecl *param = *iter;
        scope.addDirectValue(param);
    }
}

void TypeCheck::endSubroutineDefinition()
{
    declarativeRegion = declarativeRegion->getParent();
    scope.pop();
}

Node TypeCheck::acceptKeywordSelector(IdentifierInfo *key,
                                      Location        loc,
                                      Node            exprNode,
                                      bool            forSubroutine)
{
    if (!forSubroutine) {
        assert(false && "cannot accept keyword selectors for types yet!");
        return Node::getInvalidNode();
    }

    Expr *expr = cast_node<Expr>(exprNode);
    return new KeywordSelector(key, loc, expr);
}

bool TypeCheck::checkingProcedure() const
{
    return getCurrentProcedure() != 0;
}

bool TypeCheck::checkingFunction() const
{
    return getCurrentFunction() != 0;
}
