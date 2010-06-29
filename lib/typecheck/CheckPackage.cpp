//===-- typecheck/CheckPackage.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Type checking routines focusing on packages.
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

// FIXME: This method should not be needed.  Scope should provide such services.
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

bool TypeCheck::beginPackageSpec(IdentifierInfo *name, Location loc)
{
    PackageDecl *package = new PackageDecl(resource, name, loc);
    scope.push(PACKAGE_SCOPE);
    currentPackage = package;
    declarativeRegion = package;
    scope.addDirectDeclNoConflicts(currentPackage);
    return true;
}

void TypeCheck::endPackageSpec()
{
    assert(scope.getKind() == PACKAGE_SCOPE);
    scope.pop();

    PackageDecl *result = currentPackage;
    if (Decl *conflict = scope.addDirectDecl(result)) {
        // FIXME: The current package should be freed here.
        report(result->getLocation(), diag::CONFLICTING_DECLARATION)
            << result->getIdInfo() << getSourceLoc(conflict->getLocation());
    }
    else
        compUnit->addDeclaration(result);

    declarativeRegion = declarativeRegion->getParent();
    currentPackage = dyn_cast_or_null<PackageDecl>(declarativeRegion);
}

bool TypeCheck::beginPackageBody(IdentifierInfo *name, Location loc)
{
    Resolver &resolver = scope.getResolver();
    PackageDecl *package;

    // Resolve the package.
    if (!resolver.resolve(name)) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return false;
    }

    if (!(package = resolver.getDirectPackage())) {
        report(loc, diag::NOT_A_PACKAGE) << name;
        return false;
    }

    // Ensure the resolved package is declared in the current region.
    if (!package->isDeclaredIn(declarativeRegion)) {
        report(loc, diag::WRONG_LEVEL_FOR_PACKAGE_BODY);
        return false;
    }

    // Ensure the package does not have a body associated with it yet.
    if (package->hasImplementation()) {
        AddDecl *body = package->getImplementation();
        SourceLocation sloc = getSourceLoc(body->getLocation());
        report(loc, diag::PACKAGE_BODY_ALREADY_DEFINED) << name << sloc;
        return false;
    }

    // Construct the package body.  The AddDecl automatically registers itself
    // with the package.
    AddDecl *add = new AddDecl(package, loc);

    // Setup the environment.
    scope.push(PACKAGE_SCOPE);
    currentPackage = package;
    declarativeRegion = add;

    // Bring all of the packages declarations into scope.
    //
    // FIXME: Import generic parameters as well.
    // FIXME: A method should be provided by Scope to handle this.
    typedef DeclRegion::DeclIter iterator;
    for (iterator I = package->beginDecls(); I != package->endDecls(); ++I) {
        scope.addDirectDeclNoConflicts(*I);
        if (ArrayDecl *adecl = dyn_cast<ArrayDecl>(*I))
            introduceImplicitDecls(adecl);
        else if (EnumerationDecl *edecl = dyn_cast<EnumerationDecl>(*I))
            introduceImplicitDecls(edecl);
        else if (IntegerDecl *idecl = dyn_cast<IntegerDecl>(*I))
            introduceImplicitDecls(idecl);
    }
    return true;
}

void TypeCheck::endPackageBody()
{
    assert(scope.getKind() == PACKAGE_SCOPE);

    AddDecl *body = cast<AddDecl>(declarativeRegion);
    ensureExportConstraints(body);

    // A package body is a child of the declarative region of its spec.  Pop two
    // levels of declarative region.
    declarativeRegion = body->getParent()->getParent();
    currentPackage = dyn_cast_or_null<PackageDecl>(declarativeRegion);
    scope.pop();
}

bool TypeCheck::ensureExportConstraints(AddDecl *add)
{
    PackageDecl *package = add->getImplementedPackage();
    IdentifierInfo *packageName = package->getIdInfo();
    Location packageLoc = package->getLocation();

    bool allOK = true;

    // Traverse the set of declarations defined by the package specification and
    // ensure the body provides a definition.
    typedef DeclRegion::ConstDeclIter iterator;
    for (iterator I = package->beginDecls(); I != package->endDecls(); ++I) {

        // Ensure incomplete type declarations have a completion.
        if (IncompleteTypeDecl *ITD = dyn_cast<IncompleteTypeDecl>(*I)) {
            if (!ITD->hasCompletion()) {
                report(ITD->getLocation(), diag::MISSING_TYPE_COMPLETION)
                    << ITD->getIdInfo();
                allOK=false;
            }
            continue;
        }

        // Otherwise, check that all subroutine decls all have a completion.
        SubroutineDecl *decl = dyn_cast<SubroutineDecl>(*I);

        if (!decl)
            continue;

        // Check that a defining declaration was processed.
        //
        // FIXME: We need a better diagnostic here.  In particular, we should be
        // reporting the location of the declaration.
        if (!decl->getDefiningDeclaration()) {
            report(packageLoc, diag::MISSING_EXPORT)
                << packageName << decl->getIdInfo();
            allOK = false;
        }
    }
    return allOK;
}
