//===-- codegen/CodeGenCapsule.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenGeneric.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenRoutine.h"

#include <sstream>

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CodeGenCapsule::CodeGenCapsule(CodeGen &CG, DomainDecl *domain)
    : CG(CG),
      capsule(domain),
      theInstance(domain->getInstance()) { }

CodeGenCapsule::CodeGenCapsule(CodeGen &CG, DomainInstanceDecl *instance)
    : CG(CG),
      capsule(instance->getDefinition()),
      theInstance(instance) { }

CodeGenCapsule::CodeGenCapsule(CodeGen &CG, FunctorDecl *functor)
    : CG(CG),
      capsule(functor),
      theInstance(0) { }

void CodeGenCapsule::emit()
{
    // If this is a just a functor, analyze the dependents so that we can
    // emit a constructor function.
    if (!theInstance && isa<FunctorDecl>(capsule)) {
        CodeGenGeneric CGG(*this);
        CGG.analyzeDependants();
        return;
    }

    // If we are generating a parameterized instance populate the parameter map
    // with the actual parameters.
    if (generatingParameterizedInstance()) {
        FunctorDecl *functor = cast<FunctorDecl>(capsule);
        for (unsigned i = 0; i < theInstance->getArity(); ++i)
            paramMap[functor->getFormalType(i)] = theInstance->getActualParameter(i);
    }

    // Declare every subroutine in the add.
    if (AddDecl *add = capsule->getImplementation()) {
        typedef DeclRegion::DeclIter iterator;
        for (iterator I = add->beginDecls(); I != add->endDecls(); ++I) {
            if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*I)) {
                CodeGenRoutine CGR(*this);
                CGR.declareSubroutine(SR);
            }
        }
    }

    // Codegen each subroutine.
    if (AddDecl *add = capsule->getImplementation()) {
        typedef DeclRegion::DeclIter iterator;
        for (iterator I = add->beginDecls(); I != add->endDecls(); ++I) {
            if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*I)) {
                CodeGenRoutine CGR(*this);
                CGR.emitSubroutine(SR);
            }
        }
    }
}

bool CodeGenCapsule::generatingParameterizedInstance() const
{
    if (generatingInstance())
        return getInstance()->isParameterized();
    return false;
}

DomainInstanceDecl *CodeGenCapsule::getInstance()
{
    assert(generatingInstance() && "Not generating an instance!");
    return theInstance;
}

const DomainInstanceDecl *CodeGenCapsule::getInstance() const
{
    assert(generatingInstance() && "Not generating an instance!");
    return theInstance;
}

std::string CodeGenCapsule::getLinkName() const
{
    return getLinkName(capsule);
}

std::string CodeGenCapsule::getLinkName(const DomainInstanceDecl *instance,
                                        const SubroutineDecl *sr) const
{
    int index;
    std::string name = getLinkName(instance) + "__";
    name.append(getSubroutineName(sr));

    // FIXME: This is wrong.  We need to account for overloads local to the
    // definition.  We should be resolving the Domoid rather than the parent
    // declarative region.
    index = getDeclIndex(sr, sr->getParent());
    assert(index >= 0 && "getDeclIndex failed!");

    if (index) {
        std::ostringstream ss;
        ss << "__" << index;
        name += ss.str();
    }
    return name;
}

std::string CodeGenCapsule::getLinkName(const SubroutineDecl *sr) const
{
    std::string name;
    int index;

    // All subroutine decls must either be resolved within the context of a
    // domain (non-parameterized) or be an external declaration provided by an
    // instance.
    const DeclRegion *region = sr->getDeclRegion();

    if (isa<AddDecl>(region)) {
        // If the region is an AddDecl.  Ensure this subroutine is not in a
        // generic context.
        const DomainDecl *domain = dyn_cast<DomainDecl>(region->getParent());
        assert(domain && "Cannot mangle generic subroutine declarations!");
        name = getLinkName(domain);
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
    index = getDeclIndex(sr, sr->getParent());
    assert(index >= 0 && "getDeclIndex failed!");

    if (index) {
        std::ostringstream ss;
        ss << "__" << index;
        name += ss.str();
    }
    return name;
}

std::string CodeGenCapsule::getLinkName(const Domoid *domoid) const
{
    return domoid->getString();
}

std::string
CodeGenCapsule::getLinkName(const DomainInstanceDecl *instance) const
{
    assert(!instance->isDependent() &&
           "Cannot form link names for dependent instance declarations!");

    std::string name = instance->getString();
    for (unsigned i = 0; i < instance->getArity(); ++i) {
        DomainType *param =
            cast<DomainType>(instance->getActualParameter(i));
        std::ostringstream ss;

        if (param->denotesPercent()) {
            // Mangle percent nodes to the name of the current instance.
            assert(getCapsule()->getPercentType() == param &&
                   "Inconsistent context for a percent node!");
            ss << "__" << i <<  getLinkName(getInstance());
        }
        else {
            ss << "__" << i << getLinkName(param->getInstanceDecl());
            name += ss.str();
        }
    }
    return name;
}

int CodeGenCapsule::getDeclIndex(const Decl *decl, const DeclRegion *region)
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

std::string CodeGenCapsule::getSubroutineName(const SubroutineDecl *srd)
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

unsigned CodeGenCapsule::addCapsuleDependency(DomainInstanceDecl *instance)
{
    // FIXME: Assert that the given instance does not represent a percent
    // declaration.

    // If the given instance is parameterized, insert each argument as a
    // dependency, ignoring abstract domains and % (the formal parameters of a
    // functor, nor the domain itself, need recording).
    if (instance->isParameterized()) {
        typedef DomainInstanceDecl::arg_iterator iterator;
        for (iterator iter = instance->beginArguments();
             iter != instance->endArguments(); ++iter) {
            DomainType *argTy = cast<DomainType>(*iter);
            if (!(argTy->isAbstract() or argTy->denotesPercent())) {
                DomainInstanceDecl *argInstance = argTy->getInstanceDecl();
                assert(argInstance && "Bad domain type!");
                requiredInstances.insert(argInstance);
            }
        }
    }
    return requiredInstances.insert(instance);
}

DomainInstanceDecl *
CodeGenCapsule::rewriteAbstractDecl(AbstractDomainDecl *abstract) const {
    typedef ParameterMap::const_iterator iterator;
    iterator I = paramMap.find(abstract->getType());
    if (I == paramMap.end())
        return 0;
    DomainType *domTy = cast<DomainType>(I->second);
    return cast<DomainInstanceDecl>(domTy->getDomainTypeDecl());
}
