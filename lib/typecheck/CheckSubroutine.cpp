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

// There is nothing for us to do at the start of a subroutine declaration.
// Creation of the declaration itself is deferred until
// acceptSubroutineDeclaration is called.
void TypeCheck::beginSubroutineDeclaration(Descriptor &desc)
{
    assert((desc.isFunctionDescriptor() || desc.isProcedureDescriptor()) &&
           "Beginning a subroutine which is neither a function or procedure?");
}

Node TypeCheck::acceptSubroutineParameter(IdentifierInfo *formal, Location loc,
                                          Node declNode, PM::ParameterMode mode)
{
    // FIXME: The location provided here is the location of the formal, not the
    // location of the type.  The decl node here should be a ModelRef or similar
    // which encapsulates the needed location information.
    TypeDecl *tyDecl = ensureTypeDecl(declNode, loc);

    if (!tyDecl) return getInvalidNode();

    declNode.release();
    Type *paramTy = tyDecl->getType();
    ParamValueDecl *paramDecl = new ParamValueDecl(formal, paramTy, mode, loc);
    return getNode(paramDecl);
}

Node TypeCheck::acceptSubroutineDeclaration(Descriptor &desc,
                                            bool definitionFollows)
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
        if (TypeDecl *returnDecl = ensureTypeDecl(desc.getReturnType(), 0)) {
            Type *returnType = returnDecl->getType();
            routineDecl = new FunctionDecl(resource, name, location,
                                           parameters.data(), parameters.size(),
                                           returnType, region);
        }
    }
    else {
        routineDecl = new ProcedureDecl(resource, name, location,
                                        parameters.data(), parameters.size(),
                                        region);
    }
    if (!routineDecl) return getInvalidNode();

    // Ensure this new declaration does not conflict with any other currently in
    // scope.
    if (Decl *conflict = scope->addDirectDecl(routineDecl)) {
        report(location, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return getInvalidNode();
    }

    // Add the subroutine to the current declarative region and mark it as
    // immediate (e.g. not inherited).
    region->addDecl(routineDecl);
    routineDecl->setImmediate();

    // Since the declaration has been added permanently to the environment,
    // ensure the returned Node does not reclaim the decl.
    Node routine = getNode(routineDecl);
    routine.release();
    desc.release();
    return routine;
}

void TypeCheck::acceptOverrideTarget(Node qualNode,
                                     IdentifierInfo *name, Location loc,
                                     Node declarationNode)
{
    // The grammer does not enforce that the name to override must be
    // qualified.
    if (qualNode.isNull()) {
        report(loc, diag::EXPECTING_SIGNATURE_QUALIFIER) << name;
        return;
    }

    // Ensure that the qualifier resolves to a signature.
    Qualifier *qual = cast_node<Qualifier>(qualNode);
    SigInstanceDecl *sig = qual->resolve<SigInstanceDecl>();
    Location sigLoc = qual->getBaseLocation();

    if (!sig) {
        Decl *base = qual->getBaseDecl();
        report(sigLoc, diag::NOT_A_SUPERSIGNATURE) << base->getIdInfo();
        return;
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
        return;
    }

    // Depending on the kind of declaration we were supplied with, collect all
    // subroutine declarations with the given name.
    SubroutineDecl *overridingDecl =
        cast_node<SubroutineDecl>(declarationNode);
    typedef llvm::SmallVector<SubroutineDecl*, 8> TargetVector;
    TargetVector targets;
    if (isa<FunctionDecl>(overridingDecl))
        sigPercent->collectFunctionDecls(name, targets);
    else {
        assert(isa<ProcedureDecl>(overridingDecl));
        sigPercent->collectProcedureDecls(name, targets);
    }

    // If we did not resolve a single name, diagnose and return.
    if (targets.empty()) {
        report(loc, diag::NOT_A_COMPONENT_OF) << name << sig->getIdInfo();
        return;
    }

    // Create a set of rewrite rules compatable with the given signature
    // instance.
    AstRewriter rewrites(resource);
    rewrites.installRewrites(sig);
    rewrites[sigPercent->getType()] = context->getType();

    // Iterate over the set of targets.  Compare the rewritten version of each
    // target type with the type of our overriding decl.  If we find a match,
    // the construct is valid.
    SubroutineType *overridingType = overridingDecl->getType();
    for (TargetVector::iterator I = targets.begin(); I != targets.end(); ++I) {
        SubroutineDecl *targetDecl = *I;
        SubroutineType *targetType = targetDecl->getType();
        SubroutineType *rewriteType = rewrites.rewrite(targetType);
        if ((overridingType == rewriteType) &&
            overridingDecl->paramModesMatch(targetDecl)) {
            overridingDecl->setOverriddenDecl(targetDecl);
            return;
        }
    }

    // Otherwise, no match was found.
    report(loc, diag::INCOMPATABLE_OVERRIDE)
        << overridingDecl->getIdInfo() << name;
    return;
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
