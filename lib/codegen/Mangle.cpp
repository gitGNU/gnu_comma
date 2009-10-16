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
        else if (strncmp(name, "~=", 2) == 0)
            return "0nequal";
        else
            return name;
    }
}

/// \brief Returns the number of declarations which preceed \p decl in the given
/// region that have the same name.
///
/// This function is used to generate the postfix numbers used to distinguish
/// overloaded subroutines.
unsigned getOverloadIndex(const Decl *decl, const DeclRegion *region)
{
    IdentifierInfo *idInfo = decl->getIdInfo();
    unsigned result = 0;

    typedef DeclRegion::ConstDeclIter iterator;
    for (iterator I = region->beginDecls(); I != region->endDecls(); ++I) {
        if (idInfo == (*I)->getIdInfo()) {
            if (decl == (*I))
                return result;
            else
                result++;
        }
    }

    assert(false && "Decl not a member of the given region!");
}

} // end anonymous namespace.

std::string comma::mangle::getLinkName(const DomainInstanceDecl *instance,
                                       const SubroutineDecl *sr)
{
    // If the given subroutine is imported use the external name given by the
    // pragma.
    if (const PragmaImport *pragma =
        dyn_cast_or_null<PragmaImport>(sr->findPragma(pragma::Import)))
        return pragma->getExternalName();

    std::string name = getLinkName(instance) + "__";
    name.append(getSubroutineName(sr));

    // FIXME: This is wrong.  We need to account for overloads local to the
    // definition.  We should be resolving the Domoid rather than the parent
    // declarative region.
    unsigned index = getOverloadIndex(sr, sr->getParent());
    if (index) {
        std::ostringstream ss;
        ss << "__" << index;
        name += ss.str();
    }
    return name;
}

std::string comma::mangle::getLinkName(const SubroutineDecl *sr)
{
    // If the given subroutine is imported use the external name given by the
    // pragma.
    if (const PragmaImport *pragma =
        dyn_cast_or_null<PragmaImport>(sr->findPragma(pragma::Import)))
        return pragma->getExternalName();

    // All subroutine decls must either be resolved within the context of a
    // domain (non-parameterized) or be an external declaration provided by an
    // instance.
    const DeclRegion *region = sr->getDeclRegion();

    std::string name;
    if (isa<AddDecl>(region)) {
        // If the region is an AddDecl.  Ensure this subroutine is not in a
        // generic context.
        const PercentDecl *percent = dyn_cast<PercentDecl>(region->getParent());
        assert(percent && "Cannot mangle generic subroutine declarations!");
        name = getLinkName(cast<DomainDecl>(percent->getDefinition()));
    }
    else {
        // Otherwise, the region must be a DomainInstanceDecl.  Mangle the
        // instance for form the prefix.
        const DomainInstanceDecl *instance = cast<DomainInstanceDecl>(region);
        name = getLinkName(instance);
    }
    name += "__";
    name.append(getSubroutineName(sr));

    // FIXME:  This is wrong.  We need to account for overloads local to the
    // definition.  We should be resolving the Domoid rather than the parent
    // declarative region.
    unsigned index = getOverloadIndex(sr, sr->getParent());
    if (index) {
        std::ostringstream ss;
        ss << "__" << index;
        name += ss.str();
    }
    return name;
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


