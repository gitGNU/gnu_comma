//===-- codegen/CodeGenSubtype.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenRoutine.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void CodeGenRoutine::emitIntegerSubtypeDecl(IntegerSubtypeDecl *subDecl)
{
    IntegerType *intTy = subDecl->getType();

    // FIXME: If the type is staically constrained, do nothing.  This works for
    // now because we re-evaluate static types.  Soon we should compute the
    // needed values once and stash them as we do for dynamic types.
    if (intTy->isStaticallyConstrained())
        return;

    // Calculate the bounds of this subtype and associate the result.
    BoundsEmitter emitter(*this);
    emitter.synthScalarBounds(Builder, intTy);
}

void CodeGenRoutine::emitEnumSubtypeDecl(EnumSubtypeDecl *subDecl)
{
    EnumerationType *enumTy = subDecl->getType();

    // FIXME: If the type is staically constrained, do nothing.  This works for
    // now because we re-evaluate static types.  Soon we should compute the
    // needed values once and stash them as we do for dynamic types.
    if (enumTy->isStaticallyConstrained())
        return;

    // Calculate the bounds of this subtype and associate the result.
    BoundsEmitter emitter(*this);
    llvm::Value *bounds = emitter.synthScalarBounds(Builder, enumTy);
    SRF->associate(enumTy, activation::Bounds, bounds);
}


