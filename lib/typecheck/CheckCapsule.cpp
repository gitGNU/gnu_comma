//===-- typecheck/CheckCapsule.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Type checking routines focusing on capsules.
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Decl.h"
#include "comma/ast/TypeRef.h"

#include "llvm/ADT/DenseMap.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

void TypeCheck::beginCapsule()
{
    assert(scope->getLevel() == 0 && "Cannot typecheck nested capsules!");

    // Push a scope for the upcoming capsule and reset our per-capsule state.
    scope->push(MODEL_SCOPE);
    GenericFormalDecls.clear();
    declarativeRegion = 0;
    currentModel = 0;
}

void TypeCheck::endCapsule()
{
    assert(scope->getKind() == MODEL_SCOPE);
    scope->pop();

    ModelDecl *result = getCurrentModel();
    if (Decl *conflict = scope->addDirectDecl(result)) {
        // NOTE: The result model could be freed here.
        report(result->getLocation(), diag::CONFLICTING_DECLARATION)
            << result->getIdInfo() << getSourceLoc(conflict->getLocation());
    }
    else
        compUnit->addDeclaration(result);
}

void TypeCheck::beginGenericFormals()
{
    assert(GenericFormalDecls.empty() && "Formal decls already present!");
}

void TypeCheck::endGenericFormals() { }

void TypeCheck::beginFormalDomainDecl(IdentifierInfo *name, Location loc)
{
    // Create an abstract domain declaration to represent this formal.
    AbstractDomainDecl *formal = new AbstractDomainDecl(name, loc);

    // Set the current declarative region to be that of the formal.
    declarativeRegion = formal;

    // Push a scope for the declaration and add the formal itself.
    scope->push();
    scope->addDirectDeclNoConflicts(formal);
    GenericFormalDecls.push_back(formal);
}

void TypeCheck::endFormalDomainDecl()
{
    // Pop the scope for the formal decl.  We sould be back in the scope of the
    // capsule being defined.
    scope->pop();
    assert(scope->getKind() == MODEL_SCOPE);

    // Add the most recent generic declaration into the scope for the model,
    // checking for conflicts.
    if (Decl *conflict = scope->addDirectDecl(GenericFormalDecls.back())) {
        // The only conflict possible is with a pervious formal parameter.
        AbstractDomainDecl *dom = cast<AbstractDomainDecl>(conflict);
        report(dom->getLocation(), diag::DUPLICATE_FORMAL_PARAM)
            << dom->getIdInfo();
        GenericFormalDecls.pop_back();
    }
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
    assert(scope->getKind() == MODEL_SCOPE);

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
    scope->addDirectDeclNoConflicts(currentModel);
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

    // We are either processing a model or a generic formal domain.  Add the
    // signature.
    if (ModelDecl *model = getCurrentModel()) {
        model->addDirectSignature(superSig);
    }
    else {
        // FIXME: Use a cleaner interface when available.
        AbstractDomainDecl *decl = cast<AbstractDomainDecl>(declarativeRegion);
        decl->addSuperSignature(superSig);
    }

    // Bring all types defined by this super signature into scope (so that they
    // can participate in upcomming type expressions) and add them to the
    // current model.
    Sigoid *sigoid = superSig->getSigoid();
    aquireSignatureTypeDeclarations(declarativeRegion, sigoid);

    // Bring all implicit declarations defined by the super signature types into
    // scope.  This allows us to detect conflicting declarations as we process
    // the body.  All other subroutine declarations are processed after in
    // ensureNecessaryRedeclarations.
    aquireSignatureImplicitDeclarations(sigoid);
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

    ensureNecessaryRedeclarations(domain);
}

void TypeCheck::ensureNecessaryRedeclarations(DomainTypeDecl *domain)
{
    assert(isa<PercentDecl>(domain) || isa<AbstractDomainDecl>(domain));

    // We scan the set of declarations for each direct signature of the given
    // domain.  When a declaration is found which has not already been declared
    // and is not overriden, we add it on good faith that all upcoming
    // declarations will not conflict.
    //
    // When a conflict occurs (that is, when two declarations exists with the
    // same name and type) we remember which immediate declarations in the
    // domain are invalid.  Once all the declarations are processed, we remove
    // those declarations found to be in conflict.
    typedef llvm::SmallPtrSet<Decl*, 4> BadDeclSet;
    BadDeclSet badDecls;

    // An "indirect decl", in this context, is a subroutine decl which is
    // inherited from a super signature.  We maintain a map from such indirect
    // decls to the declaration node supplied by the signature.  This allows us
    // to provide diagnostics which mention the location of conflicts.
    typedef std::pair<SubroutineDecl*, SubroutineDecl*> IndirectPair;
    typedef llvm::DenseMap<SubroutineDecl*, SubroutineDecl*> IndirectDeclMap;
    IndirectDeclMap indirectDecls;

    const SignatureSet &sigset = domain->getSignatureSet();
    SignatureSet::iterator superIter = sigset.beginDirect();
    SignatureSet::iterator endSuperIter = sigset.endDirect();
    for ( ; superIter != endSuperIter; ++superIter) {
        SigInstanceDecl *super = *superIter;
        Sigoid *sigdecl = super->getSigoid();
        PercentDecl *sigPercent = sigdecl->getPercent();
        AstRewriter rewrites(resource);

        rewrites[sigdecl->getPercentType()] = domain->getType();
        rewrites.installRewrites(super);

        DeclRegion::DeclIter iter    = sigPercent->beginDecls();
        DeclRegion::DeclIter endIter = sigPercent->endDecls();
        for ( ; iter != endIter; ++iter) {
            // Only subroutine declarations need to be redeclared.
            SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*iter);
            if (!srDecl)
                continue;

            // If the routine is overriden, ignore it.
            if (domain->findOverridingDeclaration(srDecl))
                continue;

            // Rewrite the declaration to match the current models context.
            SubroutineDecl *rewriteDecl =
                makeSubroutineDecl(srDecl, rewrites, domain);
            Decl *conflict = scope->addDirectDecl(rewriteDecl);

            if (!conflict) {
                // Set the origin to point at the signature which originally
                // declared it.
                rewriteDecl->setOrigin(srDecl);
                domain->addDecl(rewriteDecl);
                indirectDecls.insert(IndirectPair(rewriteDecl, srDecl));
                continue;
            }

            // If the conflict is with respect to a TypeDecl, it must be a type
            // declared locally within the current model (since the set of types
            // provided by the super signatures have already been verified
            // consistent in aquireSignatureTypeDeclarations).  This is an
            // error.
            //
            // FIXME: assert that this is local to the current model.
            if (isa<TypeDecl>(conflict)) {
                SourceLocation sloc = getSourceLoc(srDecl->getLocation());
                report(conflict->getLocation(), diag::DECLARATION_CONFLICTS)
                    << srDecl->getIdInfo() << sloc;
                badDecls.insert(conflict);
            }

            // We currently do not support ValueDecls in models.  Therefore, the
            // conflict must denote a subroutine.
            SubroutineDecl *conflictRoutine = cast<SubroutineDecl>(conflict);

            // Ensure the parameter modes match, accounting for any overriding
            // declarations.
            if (!ensureMatchingParameterModes(conflictRoutine,
                                              srDecl, domain)) {
                badDecls.insert(conflictRoutine);
                continue;
            }

            if (conflictRoutine->keywordsMatch(rewriteDecl)) {
                // If the conflicting declaration does not have an origin
                // (meaning that is was explicitly declared by the model) map
                // its origin to that of the original subroutine provided by the
                // signature.
                if (!conflictRoutine->hasOrigin()) {
                    // FIXME: We should warn here that the "conflict"
                    // declaration is simply redundant.
                    conflictRoutine->setOrigin(srDecl);
                }
                continue;
            }

            // Otherwise, the keywords do not match.  If the conflicting
            // decl is a member of the indirect set, post a diagnostic.
            // Otherwise, the conflicting decl was declared by this model
            // and hense overrides the conflict.
            if (indirectDecls.count(conflictRoutine)) {
                Location modelLoc = domain->getLocation();
                SubroutineDecl *baseDecl =
                    indirectDecls.lookup(conflictRoutine);
                SourceLocation sloc1 =
                    getSourceLoc(baseDecl->getLocation());
                SourceLocation sloc2 =
                    getSourceLoc(srDecl->getLocation());
                report(modelLoc, diag::MISSING_REDECLARATION)
                    << srDecl->getIdInfo() << sloc1 << sloc2;
                badDecls.insert(conflictRoutine);
            }
        }
    }

    // Remove and clean up memory for each inherited node found to require a
    // redeclaration.
    for (BadDeclSet::iterator iter = badDecls.begin();
         iter != badDecls.end(); ++iter) {
        Decl *badDecl = *iter;
        domain->removeDecl(badDecl);
        delete badDecl;
    }
}

void TypeCheck::aquireSignatureTypeDeclarations(DeclRegion *region,
                                                Sigoid *sigdecl)
{
    PercentDecl *sigPercent = sigdecl->getPercent();

    DeclRegion::DeclIter I = sigPercent->beginDecls();
    DeclRegion::DeclIter E = sigPercent->endDecls();
    for ( ; I != E; ++I) {
        if (TypeDecl *tyDecl = dyn_cast<TypeDecl>(*I)) {
            if (Decl *conflict = scope->addDirectDecl(tyDecl)) {
                // FIXME: We should not error if conflict is an equivalent type
                // decl, but we do not have support for such a concept yet.
                //
                // FIXME: We need a better diagnostic which reports context.
                SourceLocation sloc = getSourceLoc(conflict->getLocation());
                report(tyDecl->getLocation(), diag::DECLARATION_CONFLICTS)
                    << tyDecl->getIdInfo() << sloc;
            }
            else
                region->addDecl(tyDecl);
        }
    }
}

void TypeCheck::aquireSignatureImplicitDeclarations(Sigoid *sigdecl)
{
    PercentDecl *sigPercent = sigdecl->getPercent();

    DeclRegion::DeclIter I = sigPercent->beginDecls();
    DeclRegion::DeclIter E = sigPercent->endDecls();
    for ( ; I != E; ++I) {
        TypeDecl *tyDecl = dyn_cast<TypeDecl>(*I);

        if (!tyDecl)
            continue;

        // Currently, only enumeration and integer decls supply implicit
        // declarations.  Bringing these declarations into scope should never
        // result in a conflict since they should all involve unique types.
        DeclRegion *region;
        if (EnumerationDecl *eDecl = dyn_cast<EnumerationDecl>(tyDecl))
            region = eDecl;
        else if (IntegerDecl *iDecl = dyn_cast<IntegerDecl>(tyDecl))
            region = iDecl;

        DeclRegion::DeclIter II = region->beginDecls();
        DeclRegion::DeclIter EE = region->endDecls();
        for ( ; II != EE; ++II)
            scope->addDirectDeclNoConflicts(*II);
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
    ensureExportConstraints(getCurrentDomoid()->getImplementation());

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

    if (TypeDecl *tyDecl = ensureTypeDecl(typeNode)) {
        Type *carrierTy = tyDecl->getType();
        CarrierDecl *carrier = new CarrierDecl(name, carrierTy, loc);
        if (Decl *conflict = scope->addDirectDecl(carrier)) {
            report(loc, diag::CONFLICTING_DECLARATION)
                << name << getSourceLoc(conflict->getLocation());
            return;
        }
        add->setCarrier(carrier);
    }
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
        Decl *decl = *iter;
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
        // definition.
        if (target) {
            Decl *candidate = add->findDecl(decl->getIdInfo(), target);
            SubroutineDecl *srDecl = dyn_cast_or_null<SubroutineDecl>(candidate);
            if (!candidate || (srDecl && !srDecl->hasBody())) {
                report(domainLoc, diag::MISSING_EXPORT)
                    << domainName << decl->getIdInfo();
                allOK = false;
            }
            // Check that the parameter mode profiles match in the case of a
            // subroutine.
            if (srDecl) {
                SubroutineDecl *targetRoutine = cast<SubroutineDecl>(decl);
                if (!ensureMatchingParameterModes(srDecl, targetRoutine))
                    allOK = false;
            }
        }
    }
    return allOK;
}
