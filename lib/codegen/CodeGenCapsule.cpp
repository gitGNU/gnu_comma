//===-- codegen/CodeGenCapsule.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenRoutine.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CodeGenCapsule::CodeGenCapsule(CodeGen &CG, Domoid *domoid)
    : CG(CG),
      capsule(domoid),
      linkName(CodeGen::getLinkName(domoid))
{
    emit();
}

void CodeGenCapsule::emit()
{
    // Declare every subroutine in the add.
    if (AddDecl *add = capsule->getImplementation()) {
        typedef DeclRegion::DeclIter iterator;
        for (iterator iter = add->beginDecls();
             iter != add->endDecls(); ++iter) {
            if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*iter)) {
                CodeGenRoutine CGR(*this);
                CGR.declareSubroutine(SR);
            }
        }
    }

    // Codegen each subroutine.
    if (AddDecl *add = capsule->getImplementation()) {
        typedef DeclRegion::DeclIter iterator;
        for (iterator iter = add->beginDecls();
             iter != add->endDecls(); ++iter) {
            if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*iter)) {
                CodeGenRoutine CGR(*this);
                CGR.emitSubroutine(SR);
            }
        }
    }
}

unsigned CodeGenCapsule::addCapsuleDependency(DomainInstanceDecl *instance)
{
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

