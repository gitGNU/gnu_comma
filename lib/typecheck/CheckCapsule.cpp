//===-- typecheck/CheckCapsule.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Type checking routines focusing on capsules.
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "TypeCheck.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DeclRewriter.h"
#include "comma/ast/TypeRef.h"

#include "llvm/ADT/DenseMap.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

void TypeCheck::beginCapsule()
{
    assert(scope.getLevel() == 0 && "Cannot typecheck nested capsules!");

    // Push a scope for the upcoming capsule and reset our per-capsule state.
    scope.push(MODEL_SCOPE);
    GenericFormalDecls.clear();
    declarativeRegion = 0;
    currentModel = 0;
}

void TypeCheck::endCapsule()
{
    if (Domoid *domoid = getCurrentDomoid())
        ensureExportConstraints(domoid->getImplementation());

    assert(scope.getKind() == MODEL_SCOPE);
    scope.pop();

    ModelDecl *result = getCurrentModel();
    if (Decl *conflict = scope.addDirectDecl(result)) {
        // NOTE: The result model could be freed here.
        report(result->getLocation(), diag::CONFLICTING_DECLARATION)
            << result->getIdInfo() << getSourceLoc(conflict->getLocation());
    }
    else {
        // Finalize all domoids.
        if (Domoid *domoid = dyn_cast<Domoid>(result))
            domoid->finalize();
        compUnit->addDeclaration(result);
    }
}

void TypeCheck::beginGenericFormals()
{
    assert(GenericFormalDecls.empty() && "Formal decls already present!");
}

void TypeCheck::endGenericFormals() { }

void TypeCheck::acceptFormalDomain(IdentifierInfo *name, Location loc,
                                   Node sigNode)
{
    AbstractDomainDecl *decl;

    if (sigNode.isNull())
        decl = new AbstractDomainDecl(resource, name, loc);
    else {
        TypeRef *ref = lift_node<TypeRef>(sigNode);

        if (!ref || !ref->referencesSigInstance()) {
            report(getNodeLoc(sigNode), diag::NOT_A_SIGNATURE);
            return;
        }

        sigNode.release();
        delete ref;
        SigInstanceDecl *sig = ref->getSigInstanceDecl();
        decl = new AbstractDomainDecl(resource, name, loc, sig);
    }


    if (scope.addDirectDecl(decl)) {
        // The only conflict possible is with respect to a previous generic
        // parameter.
        report(loc, diag::DUPLICATE_FORMAL_PARAM) << name;
        delete decl;
    }
    else
        GenericFormalDecls.push_back(decl);
}

void TypeCheck::beginDomainDecl(IdentifierInfo *name, Location loc)
{
    // If we have processed generic arguments, construct a functor, else a
    // domain.
    unsigned arity = GenericFormalDecls.size();

    if (arity == 0)
        currentModel = new DomainDecl(resource, name, loc);
    else
        currentModel = new FunctorDecl(resource, name, loc,
                                       &GenericFormalDecls[0], arity);
    initializeForModelDeclaration();
}

void TypeCheck::beginSignatureDecl(IdentifierInfo *name, Location loc)
{
    // If we have processed generic arguments, construct a variety, else a
    // signature.
    unsigned arity = GenericFormalDecls.size();

    if (arity == 0)
        currentModel = new SignatureDecl(resource, name, loc);
    else
        currentModel = new VarietyDecl(resource, name, loc,
                                       &GenericFormalDecls[0], arity);
    initializeForModelDeclaration();
}

void TypeCheck::initializeForModelDeclaration()
{
    assert(scope.getKind() == MODEL_SCOPE);

    // Set the current declarative region to be the percent node of the current
    // model.
    declarativeRegion = currentModel->getPercent();

    // For each generic formal, set its declarative region to be that the new
    // models percent node.
    unsigned arity = currentModel->getArity();
    for (unsigned i = 0; i < arity; ++i) {
        AbstractDomainDecl *formal = currentModel->getFormalDecl(i);
        formal->setDeclRegion(declarativeRegion);
    }

    // Bring the model itself into the current scope.  This should never result
    // in a conflict.
    scope.addDirectDeclNoConflicts(currentModel);
}

void TypeCheck::acceptSupersignature(Node typeNode)
{
    TypeRef *ref = cast_node<TypeRef>(typeNode);
    Location loc = ref->getLocation();
    SigInstanceDecl *superSig = ref->getSigInstanceDecl();

    // Check that the node denotes a signature.
    if (!superSig) {
        report(loc, diag::NOT_A_SIGNATURE);
        return;
    }

    Sigoid *source = superSig->getSigoid();
    ModelDecl *target = getCurrentModel();
    const SignatureSet &sourceSet = source->getSignatureSet();
    const SignatureSet &targetSet = target->getSignatureSet();

    // If the target set already contains the given signature we are done.
    if (targetSet.contains(superSig)) {
        report(loc, diag::REDUNDANT_SIGNATURE_INCLUSION)
            << superSig->getIdInfo();
        return;
    }

    // Establish rewrites corresponding to the given instance.
    AstRewriter rewrites(resource);
    rewrites.installRewrites(superSig);
    rewrites.addTypeRewrite(source->getPercentType(), target->getPercentType());

    // Iterate over the set of supersignatures provided by the source set.
    // Rewrite each to match the environment of the target.
    for (SignatureSet::iterator I = sourceSet.begin(), E = sourceSet.end();
         I != E; ++I) {
        SigInstanceDecl *candidate = rewrites.rewriteSigInstance(*I);

        // Ignore if the target set already contains the candidate.
        if (targetSet.contains(candidate))
            continue;

        // Aquire the immediate declarations provided by the candidate.
        acquireImmediateSignatureDeclarations(candidate, loc);
    }

    // Finally, introduce the direct signature itself and bring in the
    // corresponding direct declarations.
    target->addDirectSignature(superSig);
    acquireImmediateSignatureDeclarations(superSig, loc);
}

void TypeCheck::beginSignatureProfile()
{
    // Nothing to do.  The declarative region and scope of the current model or
    // formal domain is the destination of all declarations in a with
    // expression.
}

void TypeCheck::endSignatureProfile()
{
    DomainTypeDecl *domain;

    // Ensure that all ambiguous declarations are redeclared.  For now, the only
    // ambiguity that can arise is wrt conflicting argument keyword sets.
    if (ModelDecl *model = getCurrentModel())
        domain = model->getPercent();
    else
        domain = cast<AbstractDomainDecl>(declarativeRegion);
}

void TypeCheck::acquireImplicitDeclarations(Decl *decl)
{
    typedef DeclRegion::DeclIter iterator;
    DeclRegion *region = 0;

    // Resolve the decl by cases.  We do not use Decl::asDeclRegion() here since
    // since only primitive types implicitly export operations.
    if (EnumerationDecl *eDecl = dyn_cast<EnumerationDecl>(decl))
        region = eDecl;
    else if (IntegerDecl *iDecl = dyn_cast<IntegerDecl>(decl))
        region = iDecl;
    else
        return;

    iterator E = region->endDecls();
    for (iterator I = region->beginDecls(); I != E; ++I)
        scope.addDirectDeclNoConflicts(*I);
}

void TypeCheck::acquireImmediateSignatureDeclarations(SigInstanceDecl *sig,
                                                      Location loc)
{
    typedef DeclRegion::DeclIter iterator;
    PercentDecl *sigPercent = sig->getSigoid()->getPercent();
    DeclRewriter rewrites(resource, declarativeRegion, sigPercent);

    // Map the formal arguments of the signature to the actual arguments of the
    // instance, and map the percent type of the instance to the percent type of
    // the current model.
    rewrites.installRewrites(sig);
    rewrites.addTypeRewrite(sigPercent->getType(), getCurrentPercentType());

    iterator E = sigPercent->endDecls();
    for (iterator I = sigPercent->beginDecls(); I != E; ++I) {
        // Ignore any non-immediate declarations.
        if (!(*I)->isImmediate())
            continue;

        // Apply the rewrite rules, constructing a new declaration node in the
        // process.
        Decl *candidate = rewrites.rewriteDecl(*I);

        // Ensure there are no conflicts.
        if (Decl *conflict = scope.addDirectDecl(candidate)) {
            // If either the conflict or candidate is not immediate, resolve the
            // original declaration.  Non-immediate declarations are implicitly
            // generated and we want our diagnostics to point at the relevant
            // item in the source.
            conflict = conflict->resolveOrigin();
            candidate = candidate->resolveOrigin();

            // FIXME: Improve the diagnostics here.  We could look at the origin
            // of the conflicting declaration and provide a qualified name.
            //
            // Report which signature provided the conflict.
            report(loc, diag::CONFLICTING_SIGNATURE_INCLUSION)
                << sig->getIdInfo();

            // Report which declarations conflict.
            SourceLocation sloc = getSourceLoc(conflict->getLocation());
            report(candidate->getLocation(), diag::DECLARATION_CONFLICTS)
                << candidate->getIdInfo() << sloc;
        }
        else {
            // Bring any implicit declarations into scope, and add the candidate
            // to the current region.
            acquireImplicitDeclarations(candidate);
            declarativeRegion->addDecl(candidate);
        }
    }
}

void TypeCheck::beginAddExpression()
{
    Domoid *domoid = getCurrentDomoid();
    assert(domoid && "Processing `add' expression outside domain context!");

    // Switch to the declarative region which this domains AddDecl provides.
    declarativeRegion = domoid->getImplementation();
    assert(declarativeRegion && "Domain missing Add declaration node!");
}

void TypeCheck::endAddExpression()
{
    // Switch back to the declarative region of the defining domains percent
    // node.
    declarativeRegion = declarativeRegion->getParent();
    assert(declarativeRegion == getCurrentPercent()->asDeclRegion());
}

void TypeCheck::acceptCarrier(IdentifierInfo *name, Location loc, Node typeNode)
{
    // We should always be in an add declaration.
    AddDecl *add = cast<AddDecl>(declarativeRegion);

    if (add->hasCarrier()) {
        report(loc, diag::MULTIPLE_CARRIER_DECLARATIONS);
        return;
    }

    TypeDecl *tyDecl = ensureCompleteTypeDecl(typeNode);

    if (!tyDecl)
        return;

    PrimaryType *carrierTy = tyDecl->getType();

    if (carrierTy->involvesPercent()) {
        report(loc, diag::SELF_RECURSIVE_TYPE_DECLARATION);
        return;
    }

    if (carrierTy->isIndefiniteType()) {
        report(loc, diag::INDEFINITE_CARRIER_TYPE);
        return;
    }

    CarrierDecl *carrier;
    carrier = new CarrierDecl(resource, name, carrierTy, loc);
    if (Decl *conflict = scope.addDirectDecl(carrier)) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << name << getSourceLoc(conflict->getLocation());
        return;
    }
    add->setCarrier(carrier);
}

bool TypeCheck::ensureExportConstraints(AddDecl *add)
{
    Domoid *domoid = add->getImplementedDomoid();
    IdentifierInfo *domainName = domoid->getIdInfo();
    PercentDecl *percent = domoid->getPercent();
    Location domainLoc = domoid->getLocation();

    bool allOK = true;

    // The domoid contains all of the declarations inherited from the super
    // signatures and any associated with expression.  Traverse the set of
    // declarations and ensure that the AddDecl provides a definition.
    for (DeclRegion::ConstDeclIter iter = percent->beginDecls();
         iter != percent->endDecls(); ++iter) {

        // Ensure incomplete type declarations have a completion.
        if (IncompleteTypeDecl *ITD = dyn_cast<IncompleteTypeDecl>(*iter)) {
            if (!ITD->hasCompletion()) {
                report(ITD->getLocation(), diag::MISSING_TYPE_COMPLETION)
                    << ITD->getIdInfo();
                allOK=false;
            }
            continue;
        }

        // Otherwise, check that all subroutine decls all have a completion.
        SubroutineDecl *decl = dyn_cast<SubroutineDecl>(*iter);

        if (!decl)
            continue;

        // Check that a defining declaration was processed.
        //
        // FIXME: We need a better diagnostic here.  In particular, we should be
        // reporting which signature(s) demand the missing export.  One solution
        // is to use the origin link in the declaration node to locate the
        // original signature definition.
        if (!decl->getDefiningDeclaration()) {
            report(domainLoc, diag::MISSING_EXPORT)
                << domainName << decl->getIdInfo();
            allOK = false;
        }
    }
    return allOK;
}
