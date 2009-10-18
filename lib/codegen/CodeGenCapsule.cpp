//===-- codegen/CodeGenCapsule.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenCapsule.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/Mangle.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

CodeGenCapsule::CodeGenCapsule(CodeGen &CG, InstanceInfo *instance)
    : CG(CG),
      CGT(CG, instance->getInstanceDecl()),
      capsule(instance->getDefinition()),
      capsuleLinkName(instance->getLinkName()),
      theInstanceInfo(instance) { }

void CodeGenCapsule::emit()
{
    // If we are generating a parameterized instance populate the parameter map
    // with the actual parameters.
    if (generatingParameterizedInstance()) {
        FunctorDecl *functor = cast<FunctorDecl>(capsule);
        DomainInstanceDecl *instance = getInstance();
        for (unsigned i = 0; i < instance->getArity(); ++i)
            paramMap[functor->getFormalType(i)] =
                instance->getActualParamType(i);
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

    theInstanceInfo->markAsCompiled();
}

bool CodeGenCapsule::generatingParameterizedInstance() const
{
    return getInstance()->isParameterized();
}

DomainInstanceDecl *CodeGenCapsule::getInstance()
{
    return theInstanceInfo->getInstanceDecl();
}

const DomainInstanceDecl *CodeGenCapsule::getInstance() const
{
    return theInstanceInfo->getInstanceDecl();
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
