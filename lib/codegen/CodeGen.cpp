//===-- codegen/CodeGen.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/Support/Casting.h"

#include <sstream>

using namespace comma;

using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

CodeGen::CodeGen(llvm::Module *M, const llvm::TargetData &data)
    : M(M),
      TD(data),
      CGTypes(new CodeGenTypes(*this)),
      CRT(new CommaRT(*this)) { }

CodeGen::~CodeGen()
{
    delete CGTypes;
    delete CRT;
}

CodeGenTypes &CodeGen::getTypeGenerator()
{
    return *CGTypes;
}

const CodeGenTypes &CodeGen::getTypeGenerator() const
{
    return *CGTypes;
}

void CodeGen::emitToplevelDecl(Decl *decl)
{
    // Emit domains only.
    if (Domoid *domoid = dyn_cast<Domoid>(decl)) {
        CodeGenCapsule CGC(*this, domoid);
        llvm::GlobalVariable *info = CRT->registerCapsule(CGC);
        capsuleInfoTable[CGC.getLinkName()] = info;
    }
}

bool CodeGen::insertGlobal(const std::string &linkName, llvm::GlobalValue *GV)
{
    assert(GV && "Cannot insert null values into the global table!");

    if (lookupGlobal(linkName))
        return false;

    globalTable[linkName] = GV;
    return true;
}

llvm::GlobalValue *CodeGen::lookupGlobal(const std::string &linkName) const
{
    StringGlobalMap::const_iterator iter = globalTable.find(linkName);

    if (iter != globalTable.end())
        return iter->second;
    return 0;
}

llvm::GlobalValue *CodeGen::lookupCapsuleInfo(Domoid *domoid) const
{
    std::string name = getLinkName(domoid);
    StringGlobalMap::const_iterator iter = capsuleInfoTable.find(name);

    if (iter != capsuleInfoTable.end())
        return iter->second;
    else
        return 0;
}


llvm::Constant *CodeGen::emitStringLiteral(const std::string &str,
                                           bool isConstant,
                                           const std::string &name)
{
    llvm::Constant *stringConstant = llvm::ConstantArray::get(str, true);
    return new llvm::GlobalVariable(stringConstant->getType(), isConstant,
                                    llvm::GlobalValue::InternalLinkage,
                                    stringConstant, name, M);
}

std::string CodeGen::getLinkPrefix(const Decl *decl)
{
    std::string prefix;
    const DeclRegion *region = decl->getDeclRegion();
    const char *component;

    while (region) {
        if (isa<AddDecl>(region))
            region = region->getParent();

        component = cast<Decl>(region)->getString();
        prefix.insert(0, "__");
        prefix.insert(0, component);

        region = region->getParent();
    }

    return prefix;
}

std::string CodeGen::getLinkName(const SubroutineDecl *sr)
{
    std::string name;
    int index;

    name = getLinkPrefix(sr);
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

std::string CodeGen::getLinkName(const Domoid *domoid)
{
    return domoid->getString();
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
