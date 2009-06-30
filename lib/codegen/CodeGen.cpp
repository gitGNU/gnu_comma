//===-- codegen/CodeGen.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"

#include "llvm/Support/Casting.h"

#include <sstream>

using namespace comma;

using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

std::string CodeGen::getLinkName(const SubroutineDecl *sr)
{
    std::string name;
    int index;
    const DeclRegion *region = sr;
    const char *component;

    while ((region = region->getParent())) {

        if (isa<AddDecl>(region))
            region = region->getParent();

        const Decl *decl = cast<Decl>(region);
        component = decl->getString();
        name.insert(0, "__");
        name.insert(0, component);
    }

    name.append(getSubroutineName(sr));

    index = getDeclIndex(sr, sr->getParent());
    assert(index >= 0 && "getDeclIndex failed!");

    if (index) {
        std::ostringstream ss;
        ss << "__" << index;
        name += ss.str();
    }

    return name;
}

int CodeGen::getDeclIndex(const Decl *decl, const DeclRegion *region)
{
    IdentifierInfo *idInfo = decl->getIdInfo();
    unsigned result = 0;

    typedef DeclRegion::ConstDeclIter iterator;

    for (iterator i = region->beginDecls(); i != region->endDecls(); ++i) {
        if (idInfo == (*i)->getIdInfo()) {
            if (decl == (*i))
                return result;
            else
                result++;
        }
    }
    return -1;
}

std::string CodeGen::getSubroutineName(const SubroutineDecl *srd)
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
