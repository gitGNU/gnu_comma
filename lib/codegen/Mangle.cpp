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
        else if (strncmp(name, "/=", 1) == 0)
            return "0nequal";
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
        else if (strncmp(name, "~=", 2) == 0)
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
/// For the given SubroutineDecl, locates the PercentDecl corresponding the the
/// domain which defines the subroutine, and the view of the subroutine within
/// the PercentDecl (if available).
std::pair<const PercentDecl*, const SubroutineDecl*>
getPercentImage(const SubroutineDecl *srDecl)
{
    typedef std::pair<const PercentDecl*, const SubroutineDecl*> Pair;

    const DeclRegion *region = srDecl->getDeclRegion();
    const PercentDecl *percent = 0;
    const SubroutineDecl *target = 0;

    /// There are three cases.  The simplest is when the given subroutine is
    /// declared within a PercentDecl.
    if ((percent = dyn_cast<PercentDecl>(region)))
        return Pair(percent, srDecl);

    /// Next, the declaration context may be a DomainInstanceDecl.  In this case
    /// the given subroutine declaration has its origin link set directly to
    /// internal declaration in percent.
    if (isa<DomainInstanceDecl>(region)) {
        target = srDecl->getOrigin();
        percent = cast<PercentDecl>(target->getDeclRegion());
        return Pair(percent, target);
    }

    /// Finally, the given subroutine must be declared in the context of an
    /// AddDecl.  Check if srDecl is the completion of declaration in the
    /// corresponding PercentDecl and if so, return the "forward declaration".
    /// Otherwise, there is no view of the subroutine in percent and so the
    /// target is set to null.
    const AddDecl *add = cast<AddDecl>(region);
    percent = cast<PercentDecl>(add->getParent());
    target = srDecl->getForwardDeclaration();
    return Pair(percent, target);
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

    // Resolve the PercentDecl corresponding to this subroutine.
    std::pair<const PercentDecl*, const SubroutineDecl*> percentImage;
    percentImage = getPercentImage(srDecl);

    // Generate the index wrt the resolved PercentDecl.  If there is a match,
    // return the computed index.
    if (getRegionIndex(percentImage.first, idInfo,
                       percentImage.second, index))
        return index;

    // Otherwise, srDecl must denote a declaration which is private to the
    // corresponding domain.  Search the body of the domain for a match.
    const AddDecl *add = cast<AddDecl>(srDecl->getDeclRegion());
    if (getRegionIndex(add, idInfo, srDecl, index))
        return index;

    // We should always find a match.
    assert(false && "Decl not a member of its context!");
    return 0;
}

} // end anonymous namespace.

std::string comma::mangle::getLinkName(const DomainInstanceDecl *instance,
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

std::string comma::mangle::getLinkName(const SubroutineDecl *srDecl)
{
    // All subroutine decls must either be resolved within the context of a
    // domain (non-parameterized) or be an external declaration provided by an
    // instance.
    const DeclRegion *region = srDecl->getDeclRegion();
    const DomainInstanceDecl *instance;

    if (isa<AddDecl>(region))
        region = cast<PercentDecl>(region->getParent());

    if (const PercentDecl *percent = dyn_cast<PercentDecl>(region)) {
        const DomainDecl *domain = cast<DomainDecl>(percent->getDefinition());
        instance = domain->getInstance();
    }
    else
        instance = cast<DomainInstanceDecl>(region);

    return getLinkName(instance, srDecl);
}

std::string comma::mangle::getLinkName(const Domoid *domoid)
{
    return domoid->getString();
}

std::string comma::mangle::getLinkName(const DomainInstanceDecl *instance)
{
    assert(!instance->isDependent() &&
           "Cannot form link names for dependent instance declarations!");

    std::string name = instance->getString();
    for (unsigned i = 0; i < instance->getArity(); ++i) {
        DomainType *param =
            cast<DomainType>(instance->getActualParamType(i));
        std::ostringstream ss;

        if (param->denotesPercent()) {
            // Mangle percent nodes to the name of the corresponding domain.
            PercentDecl *percent = param->getPercentDecl();
            Domoid *model = cast<Domoid>(percent->getDefinition());
            ss << "__" << i <<  getLinkName(model);
        }
        else {
            ss << "__" << i << getLinkName(param->getInstanceDecl());
            name += ss.str();
        }
    }
    return name;
}

std::string comma::mangle::getLinkName(const ExceptionDecl *exception)
{
    // FIXME: Factor out and share with implementation of getLinkName for
    // subroutine declarations.
    const DeclRegion *region = exception->getDeclRegion();
    const DomainInstanceDecl *instance;
    if (isa<AddDecl>(region))
        region = cast<PercentDecl>(region->getParent());

    if (const PercentDecl *percent = dyn_cast<PercentDecl>(region)) {
        const DomainDecl *domain = cast<DomainDecl>(percent->getDefinition());
        instance = domain->getInstance();
    }
    else
        instance = cast<DomainInstanceDecl>(region);

    std::string name = getLinkName(instance) + "__";
    name.append(exception->getString());
    return name;
}
