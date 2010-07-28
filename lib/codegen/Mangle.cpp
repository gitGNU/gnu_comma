//===-- codegen/Mangle.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/Mangle.h"

#include <sstream>

using namespace comma;
using llvm::dyn_cast_or_null;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// \brief Returns the name of a subroutine, translating binary operators into a
/// unique long-form.
///
/// For example, "*" translates into "0multiply" and "+" translates into
/// "0plus".  This is a helper function for CodeGen::getLinkName.
std::string getSubroutineName(const SubroutineDecl *srd)
{
    const char *name = srd->getIdInfo()->getString();

    switch (strlen(name)) {

    default:
        return name;

    case 1:
        if (strncmp(name, "!", 1) == 0)
            return "0bang";
        else if (strncmp(name, "&", 1) == 0)
            return "0amper";
        else if (strncmp(name, "#", 1) == 0)
            return "0hash";
        else if (strncmp(name, "*", 1) == 0)
            return "0multiply";
        else if (strncmp(name, "+", 1) == 0)
            return "0plus";
        else if (strncmp(name, "-", 1) == 0)
            return "0minus";
        else if (strncmp(name, "<", 1) == 0)
            return "0less";
        else if (strncmp(name, "=", 1) == 0)
            return "0equal";
        else if (strncmp(name, ">", 1) == 0)
            return "0great";
        else if (strncmp(name, "@", 1) == 0)
            return "0at";
        else if (strncmp(name, "\\", 1) == 0)
            return "0bslash";
        else if (strncmp(name, "^", 1) == 0)
            return "0hat";
        else if (strncmp(name, "`", 1) == 0)
            return "0grave";
        else if (strncmp(name, "|", 1) == 0)
            return "0bar";
        else if (strncmp(name, "/", 1) == 0)
            return "0fslash";
        else if (strncmp(name, "~", 1) == 0)
            return "0tilde";
        else
            return name;

    case 2:
        if (strncmp(name, "<=", 2) == 0)
            return "0leq";
        else if (strncmp(name, "<>", 2) == 0)
            return "0diamond";
        else if (strncmp(name, "<<", 2) == 0)
            return "0dless";
        else if (strncmp(name, "==", 2) == 0)
            return "0dequal";
        else if (strncmp(name, ">=", 2) == 0)
            return "0geq";
        else if (strncmp(name, ">>", 2) == 0)
            return "0dgreat";
        else if  (strncmp(name, "||", 2) == 0)
            return "0dbar";
        else if (strncmp(name, "/=", 2) == 0)
            return "0nequal";
        else
            return name;
    }
}

/// \brief Helper function for getOverloadIndex.
///
/// Counts the number of declarations with the given defining identifier which
/// precede \p target in the given DeclRegion.
///
/// \param region The declarative region to scan.
///
/// \param idInfo The defining identifier to match against.
///
/// \param target A declaration to use as a termination condition.  This pointer
/// may be null to support a scan of the entire region (and is the reason for
/// splitting \p idInfo and \p target into separate arguments).
///
/// \param index Incremented by the number of declarations which preceded \p
/// target with the given name.
///
/// \return true if a matching declaration was found in the region and false
/// otherwise.
unsigned getRegionIndex(const DeclRegion *region, IdentifierInfo *idInfo,
                        const Decl *target, unsigned &index)
{
    typedef DeclRegion::ConstDeclIter iterator;
    iterator I = region->beginDecls();
    iterator E = region->endDecls();

    for ( ; I != E; ++I) {
        const Decl *candidate = *I;
        if (idInfo == candidate->getIdInfo()) {
            // If the target matches the current declaration, terminate the
            // index count.
            if (target == candidate)
                return true;

            // Do not increment the index if a foward declaration exists (we
            // would be counting the same declaration twice).
            if (const SubroutineDecl *SR = dyn_cast<SubroutineDecl>(candidate))
                if (SR->hasForwardDeclaration())
                    continue;

            index++;
        }
    }
    return false;
}

/// \brief Helper function for getOverloadIndex().
///
/// For the given SubroutineDecl, locates the PackageDecl of the package which
/// defines the subroutine, and the view of the subroutine within the
/// PackageDecl (if available).
std::pair<const DeclRegion*, const SubroutineDecl*>
getPublicRegion(const SubroutineDecl *srDecl)
{
    typedef std::pair<const DeclRegion*, const SubroutineDecl*> Pair;

    const DeclRegion *region = srDecl->getDeclRegion();

    /// Check if the region already corresponds to the public exports.
    if (isa<PackageDecl>(region))
        return Pair(region, srDecl);

    /// If the declaration context is a PkgInstanceDecl the given subroutine
    /// declaration has its origin link set directly to the original
    /// declaration.
    if (isa<PkgInstanceDecl>(region)) {
        const SubroutineDecl *target = srDecl->getOrigin();
        return Pair(target->getDeclRegion(), target);
    }

    /// If the declaration is declared in the private part of a package it is
    /// never a member of the public view.
    if (isa<PrivatePart>(region))
        return Pair(region->getParent(), 0);

    /// Finally, the given subroutine must be declared in the context of an
    /// BodyDecl.  Check if srDecl is the completion of a declaration in the
    /// corresponding public or private view (the parent region) and if so,
    /// return the "forward declaration".
    assert(isa<BodyDecl>(region));
    region = region->getParent();
    if (isa<PrivatePart>(region))
        region = region->getParent();
    return Pair(region, srDecl->getForwardDeclaration());
}

/// \brief Returns the number of declarations which precede \p srDecl.
///
/// This function is used to generate the postfix numbers used to distinguish
/// overloaded subroutines.
///
/// NOTE: This is an expensive function to call.
unsigned getOverloadIndex(const SubroutineDecl *srDecl)
{
    unsigned index = 0;
    IdentifierInfo *idInfo = srDecl->getIdInfo();

    // Resolve the DeclRegion corresponding to this subroutine.
    std::pair<const DeclRegion*, const SubroutineDecl*> lookup;
    const PackageDecl *package;
    const SubroutineDecl *target;
    lookup = getPublicRegion(srDecl);
    package = cast<PackageDecl>(lookup.first);
    target = lookup.second;

    // Generate the index wrt the resolved PackageDecl.  If there is a match,
    // return the computed index.
    if (target && getRegionIndex(package, idInfo, target, index))
        return index;

    // Next, scan the private part of the package if available.
    if (const PrivatePart *ppart = package->getPrivatePart()) {
        if (getRegionIndex(ppart, idInfo, srDecl, index))
            return index;
    }

    // Otherwise, srDecl must denote a declaration which is private to the
    // corresponding package.  Search the body of the package for a match.
    const BodyDecl *body = cast<BodyDecl>(srDecl->getDeclRegion());
    if (getRegionIndex(body, idInfo, srDecl, index))
        return index;

    // We should always find a match.
    assert(false && "Decl not a member of its context!");
    return 0;
}

} // end anonymous namespace.

std::string comma::mangle::getLinkName(const PkgInstanceDecl *instance,
                                       const SubroutineDecl *srDecl)
{
    // If the given subroutine is imported use the external name given by the
    // pragma.
    if (const PragmaImport *pragma =
        dyn_cast_or_null<PragmaImport>(srDecl->findPragma(pragma::Import)))
        return pragma->getExternalName();

    std::string name = getLinkName(instance) + "__";
    name.append(getSubroutineName(srDecl));

    unsigned index = getOverloadIndex(srDecl);
    if (index) {
        std::ostringstream ss;
        ss << "__" << index;
        name += ss.str();
    }
    return name;
}

std::string comma::mangle::getLinkName(const PkgInstanceDecl *instance)
{
    return instance->getDefinition()->getString();
}

std::string comma::mangle::getLinkName(const ExceptionDecl *exception)
{
    // FIXME: Factor out and share with implementation of getLinkName for
    // subroutine declarations.
    const DeclRegion *region = exception->getDeclRegion();
    const PkgInstanceDecl *instance;

    if (isa<BodyDecl>(region))
        region = region->getParent();

    if (const PackageDecl *package = dyn_cast<PackageDecl>(region))
        instance = package->getInstance();
    else
        instance = cast<PkgInstanceDecl>(region);

    std::string name = getLinkName(instance) + "__";
    name.append(exception->getString());
    return name;
}
