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
    if (AddDecl *add = capsule->getImplementation()) {
        typedef DeclRegion::DeclIter iterator;
        for (iterator iter = add->beginDecls();
             iter != add->endDecls(); ++iter) {
            if (SubroutineDecl *SR = dyn_cast<SubroutineDecl>(*iter))
                CodeGenRoutine CGR(*this, SR);
        }
    }
}
