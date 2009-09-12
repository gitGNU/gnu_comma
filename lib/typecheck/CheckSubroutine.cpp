//===-- CheckSubroutine.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Type check logic supporting subroutine declarations and definitions.
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Qualifier.h"
#include "comma/ast/Stmt.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

bool TypeCheck::ensureMatchingParameterModes(SubroutineDecl *X,
                                             SubroutineDecl *Y)
{
    unsigned arity = X->getArity();
    assert(arity == Y->getArity() && "Arity mismatch!");

    for (unsigned i = 0; i < arity; ++i) {
        if (X->getParamMode(i) != Y->getParamMode(i)) {
            ParamValueDecl *param = X->getParam(i);
            report(param->getLocation(),
                   diag::INCOMPATABLE_MODE_REDECLARATION)
                << getSourceLoc(Y->resolveOrigin()->getLocation());
            return false;
        }
    }
    return true;
}

bool TypeCheck::ensureMatchingParameterModes(
    SubroutineDecl *X, SubroutineDecl *Y, DeclRegion *region)
{
    unsigned arity = X->getArity();
    assert(arity == Y->getArity() && "Arity mismatch!");

    for (unsigned i = 0; i < arity; ++i) {
        if (X->getParamMode(i) != Y->getParamMode(i)) {
            // The parameter modes do not match.  Using the supplied declarative
            // region, check if any overriding declarations are in effect.
            if (region->findOverridingDeclaration(Y))
                return false;

            // None were found.
            if (X->isImmediate()) {
                // X is an immediate decl.  Use the location of the offending
                // parameter as a context.
                ParamValueDecl *param = X->getParam(i);
                report(param->getLocation(),
                       diag::INCOMPATABLE_MODE_REDECLARATION)
                    << getSourceLoc(Y->getLocation());
            }
            else {
                // Otherwise, resolve the declarative region to a PercentDecl or
                // AbstractDomainDecl and use the corresponding model as
                // context.  Form a cross reference diagnostic involving the
                // origin of X.
                Location contextLoc;
                if (PercentDecl *context = dyn_cast<PercentDecl>(region))
                    contextLoc = context->getDefinition()->getLocation();
                else {
                    AbstractDomainDecl *context =
                        cast<AbstractDomainDecl>(region);
                    contextLoc = context->getLocation();
                }
                report(contextLoc, diag::SUBROUTINE_OVERRIDE_REQUIRED)
                    << X->getIdInfo()
                    << getSourceLoc(X->getOrigin()->getLocation())
                    << getSourceLoc(Y->getLocation());
            }
            return false;
        }
    }
    return true;
}

void TypeCheck::beginFunctionDeclaration(IdentifierInfo *name, Location loc)
{
    assert(srProfileInfo.kind == SubroutineProfileInfo::EMPTY_PROFILE &&
           "Subroutine profile info already initialized!");

    srProfileInfo.kind = SubroutineProfileInfo::FUNCTION_PROFILE;
    srProfileInfo.name = name;
    srProfileInfo.loc = loc;
}

void TypeCheck::beginProcedureDeclaration(IdentifierInfo *name, Location loc)
{
    assert(srProfileInfo.kind == SubroutineProfileInfo::EMPTY_PROFILE &&
           "Subroutine profile info already initialized!");

    srProfileInfo.kind = SubroutineProfileInfo::PROCEDURE_PROFILE;
    srProfileInfo.name = name;
    srProfileInfo.loc = loc;
}

void TypeCheck::acceptSubroutineParameter(IdentifierInfo *formal, Location loc,
                                          Node declNode, PM::ParameterMode mode)
{
    assert(srProfileInfo.kind != SubroutineProfileInfo::EMPTY_PROFILE &&
           "Subroutine profile not initialized!");

    // FIXME: The location provided here is the location of the formal, not the
    // location of the type.  The decl node here should be a ModelRef or similar
    // which encapsulates the needed location information.
    TypeDecl *tyDecl = ensureTypeDecl(declNode, loc);

    if (!tyDecl) {
        srProfileInfo.markInvalid();
        return;
    }

    // If we are building a function declaration, ensure that the parameter is
    // of mode "in".
    if (srProfileInfo.denotesFunction() &&
        (mode != PM::MODE_IN) && (mode != PM::MODE_DEFAULT)) {
        report(loc, diag::OUT_MODE_IN_FUNCTION);
        srProfileInfo.markInvalid();
        return;
    }

    // Check that this parameters name does not conflict with any previous
    // parameters.
    typedef SubroutineProfileInfo::ParamVec::iterator iterator;
    for (iterator I = srProfileInfo.params.begin();
         I != srProfileInfo.params.end(); ++I) {
        ParamValueDecl *previousParam = *I;
        if (previousParam->getIdInfo() == formal) {
            report(loc, diag::DUPLICATE_FORMAL_PARAM) << formal;
            srProfileInfo.markInvalid();
            return;
        }
    }

    // Check that the parameter name does not conflict with the subroutine
    // declaration itself.
    if (formal == srProfileInfo.name) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << formal << getSourceLoc(srProfileInfo.loc);
        srProfileInfo.markInvalid();
        return;
    }

    declNode.release();
    Type *paramTy = tyDecl->getType();
    ParamValueDecl *paramDecl = new ParamValueDecl(formal, paramTy, mode, loc);
    srProfileInfo.params.push_back(paramDecl);
}

void TypeCheck::acceptFunctionReturnType(Node typeNode)
{
    assert(srProfileInfo.denotesFunction() &&
           "Inconsitent state for function returns!");

    if (typeNode.isNull()) {
        srProfileInfo.markInvalid();
        return;
    }

    TypeDecl *returnDecl = ensureTypeDecl(typeNode, 0);
    if (!returnDecl) {
        srProfileInfo.markInvalid();
        return;
    }

    typeNode.release();
    srProfileInfo.returnTy = returnDecl;
}

void TypeCheck::acceptOverrideTarget(Node qualNode,
                                     IdentifierInfo *name, Location loc)
{
    // Simply store the given info into the current profile info.  We check
    // overrides once the declaration node has been built.
    if (qualNode.isNull())
        srProfileInfo.overrideQual = 0;
    else
        srProfileInfo.overrideQual = cast_node<Qualifier>(qualNode);

    srProfileInfo.overrideName = name;
    srProfileInfo.overrideLoc = loc;
}

Node TypeCheck::endSubroutineDeclaration(bool definitionFollows)
{
    IdentifierInfo *name = srProfileInfo.name;
    Location location = srProfileInfo.loc;
    SubroutineProfileInfo::ParamVec &params = srProfileInfo.params;

    // Ensure the profile info is reset once this method returns.
    SubroutineProfileInfoReseter reseter(srProfileInfo);

    // If the subroutine profile has not checked out thus far, do not construct
    // a subroutine declaration for it.
    if (srProfileInfo.isInvalid())
        return getInvalidNode();

    // Ensure that if this function names a binary or unary operator it has the
    // required arity.
    if (srProfileInfo.denotesFunction()) {
        if (namesUnaryFunction(name)) {
            if (params.size() != 1 && !namesBinaryFunction(name))
                report(location, diag::OPERATOR_ARITY_MISMATCH) << name;
        }
        if (namesBinaryFunction(name)) {
            if (params.size() != 2) {
                report(location, diag::OPERATOR_ARITY_MISMATCH) << name;
                return getInvalidNode();
            }
        }
    }

    SubroutineDecl *routineDecl = 0;
    DeclRegion *region = currentDeclarativeRegion();
    if (srProfileInfo.denotesFunction()) {
        Type *returnType = srProfileInfo.returnTy->getType();
        routineDecl = new FunctionDecl(resource, name, location,
                                       params.data(), params.size(),
                                       returnType, region);
    }
    else
        routineDecl = new ProcedureDecl(resource, name, location,
                                        params.data(), params.size(),
                                        region);

    // If this declaration overrides, validate it using the data in
    // srProfileInfo.
    if (!validateOverrideTarget(routineDecl))
        return getInvalidNode();

    // Ensure this new declaration does not conflict with any other currently in
    // scope.
    if (Decl *conflict = scope->addDirectDecl(routineDecl)) {

        // If the conflict is a subroutine, ensure the current decl forms its
        // completion.
        SubroutineDecl *fwdDecl = dyn_cast<SubroutineDecl>(conflict);
        if (!(fwdDecl && definitionFollows &&
              compatibleSubroutineDecls(fwdDecl, routineDecl))) {
            report(location, diag::CONFLICTING_DECLARATION)
                << name << getSourceLoc(conflict->getLocation());
            return getInvalidNode();
        }
    }

    // Add the subroutine to the current declarative region and mark it as
    // immediate (e.g. not inherited).
    region->addDecl(routineDecl);
    routineDecl->setImmediate();

    // Since the declaration has been added permanently to the environment,
    // ensure the returned Node does not reclaim the decl.
    Node routine = getNode(routineDecl);
    routine.release();
    return routine;
}

bool TypeCheck::validateOverrideTarget(SubroutineDecl *overridingDecl)
{
    Qualifier *targetQual = srProfileInfo.overrideQual;
    IdentifierInfo *targetName = srProfileInfo.overrideName;
    Location targetLoc = srProfileInfo.overrideLoc;

    // If the current profile information does not name a target, there is no
    // overriding decl to check.
    if (targetName == 0)
        return true;

    // The grammer does not enforce that the name to override must be qualified.
    if (targetQual == 0) {
        report(targetLoc, diag::EXPECTING_SIGNATURE_QUALIFIER) << targetName;
        return false;
    }

    // Ensure that the qualifier resolves to a signature.
    SigInstanceDecl *sig = targetQual->resolve<SigInstanceDecl>();
    Location sigLoc = targetQual->getBaseLocation();

    if (!sig) {
        Decl *base = targetQual->getBaseDecl();
        report(sigLoc, diag::NOT_A_SUPERSIGNATURE) << base->getIdInfo();
        return false;
    }
    PercentDecl *sigPercent = sig->getSigoid()->getPercent();

    // Resolve the current context and ensure the given instance denotes a super
    // signature.
    DomainTypeDecl *context;
    if (PercentDecl *percent = getCurrentPercent())
        context = percent;
    else {
        // FIXME: Use a cleaner interface when available.
        context = cast<AbstractDomainDecl>(declarativeRegion);
    }

    if (!context->getSignatureSet().contains(sig)) {
        report(sigLoc, diag::NOT_A_SUPERSIGNATURE) << sig->getIdInfo();
        return false;
    }

    // Collect all subroutine declarations of the appropriate kind with the
    // given name.
    typedef llvm::SmallVector<SubroutineDecl*, 8> TargetVector;
    TargetVector targets;
    if (srProfileInfo.denotesFunction())
        sigPercent->collectFunctionDecls(targetName, targets);
    else {
        assert(srProfileInfo.denotesProcedure());
        sigPercent->collectProcedureDecls(targetName, targets);
    }

    // If we did not resolve a single name, diagnose and return.
    if (targets.empty()) {
        report(targetLoc, diag::NOT_A_COMPONENT_OF)
            << targetName << sig->getIdInfo();
        return false;
    }

    // Create a set of rewrite rules compatable with the given signature
    // instance.
    AstRewriter rewrites(resource);
    rewrites.installRewrites(sig);
    rewrites[sigPercent->getType()] = context->getType();

    // Iterate over the set of targets.  Compare the rewritten version of each
    // target type with the type of the overriding decl.  If we find a match,
    // the construct is valid.
    SubroutineType *overridingType = overridingDecl->getType();
    for (TargetVector::iterator I = targets.begin(); I != targets.end(); ++I) {
        SubroutineDecl *targetDecl = *I;
        SubroutineType *targetType = targetDecl->getType();
        SubroutineType *rewriteType = rewrites.rewrite(targetType);
        if ((overridingType == rewriteType) &&
            overridingDecl->paramModesMatch(targetDecl)) {
            overridingDecl->setOverriddenDecl(targetDecl);
            return true;
        }
    }

    // Otherwise, no match was found.
    report(targetLoc, diag::INCOMPATABLE_OVERRIDE)
        << overridingDecl->getIdInfo() << targetName;
    return false;
}

void TypeCheck::beginSubroutineDefinition(Node declarationNode)
{
    SubroutineDecl *srDecl = cast_node<SubroutineDecl>(declarationNode);
    declarationNode.release();

    // Enter a scope for the subroutine definition.  Add the subroutine itself
    // as an element of the new scope and add the formal parameters.  This
    // should never result in conflicts.
    scope->push(FUNCTION_SCOPE);
    scope->addDirectDeclNoConflicts(srDecl);
    typedef SubroutineDecl::param_iterator param_iterator;
    for (param_iterator I = srDecl->begin_params();
         I != srDecl->end_params(); ++I)
        scope->addDirectDeclNoConflicts(*I);

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


// Creates a procedure or function decl depending on the kind of the
// supplied type.
SubroutineDecl *
TypeCheck::makeSubroutineDecl(SubroutineDecl *SRDecl,
                              const AstRewriter &rewrites, DeclRegion *region)
{
    IdentifierInfo *name = SRDecl->getIdInfo();
    SubroutineType *SRType = rewrites.rewrite(SRDecl->getType());
    unsigned arity = SRDecl->getArity();

    llvm::SmallVector<IdentifierInfo*, 8> keys;
    for (unsigned i = 0; i < arity; ++i)
        keys.push_back(SRDecl->getParamKeyword(i));

    SubroutineDecl *result;
    if (FunctionType *ftype = dyn_cast<FunctionType>(SRType))
        result =  new FunctionDecl(name, 0, keys.data(), ftype, region);
    else {
        ProcedureType *ptype = cast<ProcedureType>(SRType);
        result = new ProcedureDecl(name, 0, keys.data(), ptype, region);
    }

    // Ensure the result declaration has the same parameter modes as the
    // original;
    for (unsigned i = 0; i < arity; ++i) {
        ParamValueDecl *param = result->getParam(i);
        param->setParameterMode(SRDecl->getExplicitParamMode(i));
    }

    return result;
}

bool
TypeCheck::compatibleSubroutineDecls(SubroutineDecl *X, SubroutineDecl *Y)
{
    if (X->getIdInfo() != Y->getIdInfo())
        return false;

    if (X->getType() != Y->getType())
        return false;

    if (!X->paramModesMatch(Y))
        return false;

    if (!X->keywordsMatch(Y))
        return false;

    return true;
}
