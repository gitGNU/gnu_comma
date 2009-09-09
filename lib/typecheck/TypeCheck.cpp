//===-- typecheck/TypeCheck.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DeclProducer.h"
#include "Scope.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Qualifier.h"
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
      declProducer(new DeclProducer(resource))
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
    // We are either processing a model or a generic formal domain.
    //
    // When processing a model, return the associated percent decl.  When
    // processing a generic formal domain, return the AbstractDomainDecl.
    if (ModelDecl *model = getCurrentModel()) {
        return getNode(model->getPercent());
    }
    else {
        // FIXME: Use a cleaner interface when available.
        AbstractDomainDecl *decl = cast<AbstractDomainDecl>(declarativeRegion);
        return getNode(decl);
    }
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
        Scope::Resolver &resolver = scope->getResolver();
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

Node TypeCheck::acceptTypeName(IdentifierInfo *id, Location loc, Node qualNode)
{
    Decl *decl;

    if (qualNode.isNull()) {
        // FIXME:  Use a Scope::Resolver here.
        decl = resolveTypeOrModelDecl(id, loc);
    }
    else {
        Qualifier *qualifier = cast_node<Qualifier>(qualNode);
        DeclRegion *region = resolveVisibleQualifiedRegion(qualifier);
        decl = resolveTypeOrModelDecl(id, loc, region);
    }

    if (decl == 0)
        return getInvalidNode();

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
        return getNode(domDecl->getInstance());
    }

    case Ast::AST_SignatureDecl: {
        SignatureDecl *sigDecl = cast<SignatureDecl>(decl);
        return getNode(sigDecl->getInstance());
    }

    case Ast::AST_AbstractDomainDecl:
    case Ast::AST_CarrierDecl:
    case Ast::AST_EnumerationDecl:
    case Ast::AST_IntegerDecl:
        return getNode(decl);

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
    Scope::Resolver &resolver = scope->getResolver();
    if (!resolver.resolve(connective) || !resolver.hasDirectCapsule()) {
        report(loc, diag::TYPE_NOT_VISIBLE) << connective;
        return getInvalidNode();
    }

    ModelDecl *model = resolver.getDirectCapsule();
    unsigned numArgs = argumentNodes.size();
    assert(numKeywords <= numArgs && "More keywords than arguments!");

    if (!model->isParameterized() || model->getArity() != numArgs) {
        report(loc, diag::WRONG_NUM_ARGS_FOR_TYPE) << connective;
        return getInvalidNode();
    }

    unsigned numPositional = numArgs - numKeywords;
    llvm::SmallVector<DomainTypeDecl*, 4> arguments(numArgs);

    // First, populate the argument vector with any positional parameters.
    //
    // FIXME: Currently only DomainTypeDecls and SigInstanceDecls propagate as
    // arguments to a type application.  We should factor this out into a
    // seperate pass over the argument vector.
    for (unsigned i = 0; i < numPositional; ++i) {
        DomainTypeDecl *arg = lift_node<DomainTypeDecl>(argumentNodes[i]);
        if (!arg) {
            SigInstanceDecl *sig =
                cast_node<SigInstanceDecl>(argumentNodes[i]);
            Location loc = argumentLocs[i];
            report(loc, diag::SIGNATURE_AS_TYPE_PARAM) << sig->getIdInfo();
            return getInvalidNode();
        }
        arguments[i] = arg;
    }

    // Process any keywords provided.
    for (unsigned i = 0; i < numKeywords; ++i) {
        IdentifierInfo *keyword = keywords[i];
        Location keywordLoc = keywordLocs[i];
        int keywordIdx = model->getKeywordIndex(keyword);

        // Ensure the given keyword exists.
        if (keywordIdx < 0) {
            report(keywordLoc, diag::TYPE_HAS_NO_SUCH_KEYWORD)
                << keyword << connective;
            return getInvalidNode();
        }

        // The corresponding index of the keyword must be greater than the
        // number of supplied positional parameters (otherwise it would
        // `overlap' a positional parameter).
        if ((unsigned)keywordIdx < numPositional) {
            report(keywordLoc, diag::PARAM_PROVIDED_POSITIONALLY) << keyword;
            return getInvalidNode();
        }

        // Ensure that this keyword is not a duplicate of any preceeding
        // keyword.
        for (unsigned j = 0; j < i; ++j) {
            if (keywords[j] == keyword) {
                report(keywordLoc, diag::DUPLICATE_KEYWORD) << keyword;
                return getInvalidNode();
            }
        }

        // Lift the argument node and add it to the set of arguments in its
        // proper position.
        unsigned argIdx = i + numPositional;
        DomainTypeDecl *argument =
            lift_node<DomainTypeDecl>(argumentNodes[argIdx]);

        // FIXME: Currently only DomainTypeDecls and SigInstanceDecls propagate
        // as arguments to a type application.  We should factor this out into a
        // seperate pass over the argument vector.
        if (!argument) {
            SigInstanceDecl *sig =
                cast_node<SigInstanceDecl>(argumentNodes[argIdx]);
            Location loc = argumentLocs[argIdx];
            report(loc, diag::SIGNATURE_AS_TYPE_PARAM) << sig->getIdInfo();
            return getInvalidNode();
        }

        argumentNodes[i + numPositional].release();
        arguments[keywordIdx] = argument;
    }

    // Check each argument type.
    //
    // FIXME:  Factor this out.
    for (unsigned i = 0; i < numArgs; ++i) {
        DomainType *argTy = arguments[i]->getType();
        Location argLoc = argumentLocs[i];
        AbstractDomainDecl *target = model->getFormalDecl(i);

        // Establish a rewriter mapping all previous formals to the given
        // actuals, and from the target to the argument (abstract domain decls
        // have % rewritten to denote themselves, which in this case we want to
        // map to the type of the actual).
        AstRewriter rewrites(resource);
        rewrites[target->getType()] = arguments[i]->getType();
        for (unsigned j = 0; j < i; ++j)
            rewrites[model->getFormalType(j)] = arguments[j]->getType();

        if (!checkSignatureProfile(rewrites, argTy, target, argLoc))
            return getInvalidNode();
    }

    // Obtain a memoized type node for this particular argument set.
    Node node = getInvalidNode();
    if (VarietyDecl *variety = dyn_cast<VarietyDecl>(model))
        node = getNode(variety->getInstance(arguments.data(), numArgs));
    else {
        FunctorDecl *functor = cast<FunctorDecl>(model);

        if (!ensureNonRecursiveInstance(
                functor, arguments.data(), numArgs, loc))
            return getInvalidNode();

        if (denotesFunctorPercent(functor, arguments.data(), numArgs)) {
            // Cannonicalize type applications which are equivalent to `%'.
            report(loc, diag::PERCENT_EQUIVALENT);
            node = getNode(getCurrentPercent());
        }
        else {
            DomainInstanceDecl *instance =
                functor->getInstance(arguments.data(), numArgs);
            node = getNode(instance);
        }
    }
    argumentNodes.release();
    return node;
}

TypeDecl *TypeCheck::ensureTypeDecl(Decl *decl, Location loc, bool report)
{
    if (TypeDecl *tyDecl = dyn_cast<TypeDecl>(decl))
        return tyDecl;
    if (report)
        this->report(loc, diag::TYPE_CANNOT_DENOTE_VALUE);
    return 0;
}

TypeDecl *TypeCheck::ensureTypeDecl(Node node, Location loc, bool report)
{
    Decl *decl = cast_node<Decl>(node);
    return ensureTypeDecl(decl, loc, report);
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

bool TypeCheck::acceptObjectDeclaration(Location loc, IdentifierInfo *name,
                                        Node declNode, Node initializerNode)
{
    Expr *init = 0;
    TypeDecl *tyDecl = ensureTypeDecl(declNode, loc);

    if (!tyDecl) return false;

    if (!initializerNode.isNull()) {
        init = cast_node<Expr>(initializerNode);
        if (!checkExprInContext(init, tyDecl->getType()))
            return false;
    }
    ObjectDecl *decl = new ObjectDecl(name, tyDecl->getType(), loc, init);

    declNode.release();
    initializerNode.release();

    if (Decl *conflict = scope->addDirectDecl(decl)) {
        SourceLocation sloc = getSourceLoc(conflict->getLocation());
        report(loc, diag::DECLARATION_CONFLICTS) << name << sloc;
        return false;
    }
    currentDeclarativeRegion()->addDecl(decl);
    return true;
}

bool TypeCheck::acceptImportDeclaration(Node importedNode, Location loc)
{
    Decl *decl = cast_node<Decl>(importedNode);
    DomainType *domain;

    if (CarrierDecl *carrier = dyn_cast<CarrierDecl>(decl))
        domain = dyn_cast<DomainType>(carrier->getRepresentationType());
    else
        domain = dyn_cast<DomainTypeDecl>(decl)->getType();

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
    EnumerationDecl *enumeration = new EnumerationDecl(name, loc, region);
    if (Decl *conflict = scope->addDirectDecl(enumeration)) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return getInvalidNode();
    }
    return getNode(enumeration);
}

void TypeCheck::acceptEnumerationLiteral(Node enumerationNode,
                                         IdentifierInfo *name, Location loc)
{
    EnumerationDecl *enumeration = cast_node<EnumerationDecl>(enumerationNode);

    if (enumeration->containsDecl(name)) {
        report(loc, diag::MULTIPLE_ENUMERATION_LITERALS) << name;
        return;
    }
    new EnumLiteral(resource, name, loc, enumeration);
}

void TypeCheck::endEnumerationType(Node enumerationNode)
{
    DeclRegion *region = currentDeclarativeRegion();
    EnumerationDecl *enumeration = cast_node<EnumerationDecl>(enumerationNode);

    enumerationNode.release();
    declProducer->createImplicitDecls(enumeration);
    region->addDecl(enumeration);
    importDeclRegion(enumeration);
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

    if (Decl *conflict = scope->addDirectDecl(Idecl)) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return;
    }
    region->addDecl(Idecl);
    declProducer->createImplicitDecls(Idecl);
    importDeclRegion(Idecl);
}

bool TypeCheck::checkType(Type *source, SigInstanceDecl *target, Location loc)
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

bool TypeCheck::checkSignatureProfile(const AstRewriter &rewrites,
                                      Type *source, AbstractDomainDecl *target,
                                      Location loc)
{
    if (DomainType *domain = dyn_cast<DomainType>(source)) {
        if (!has(rewrites, domain, target)) {
            report(loc, diag::DOES_NOT_SATISFY)
                << domain->getString()  << target->getString();
            return false;
        }
        return true;
    }

    if (CarrierType *carrier = dyn_cast<CarrierType>(source)) {
        Type *rep = dyn_cast<DomainType>(carrier->getRepresentationType());
        return checkSignatureProfile(rewrites, rep, target, loc);
    }

    // Otherwise, the source does not denote a domain, and so cannot satisfy the
    // signature constraint.
    report(loc, diag::NOT_A_DOMAIN);
    return false;
}

void TypeCheck::importDeclRegion(DeclRegion *region)
{
    // FIXME: We should be able to import a region directly into a scope, thus
    // making these declarations indirect.  However, we do not have appropriate
    // scope API's yet.
    typedef DeclRegion::DeclIter iterator;

    for (iterator I = region->beginDecls(); I != region->endDecls(); ++I) {
        Decl *decl = *I;
        if (Decl *conflict = scope->addDirectDecl(decl)) {
            report(decl->getLocation(), diag::CONFLICTING_DECLARATION)
                << decl->getIdInfo() << getSourceLoc(conflict->getLocation());
        }
    }
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
