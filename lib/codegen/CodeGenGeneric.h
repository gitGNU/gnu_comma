//===-- codegen/CodeGenGeneric.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Codegen operations specific to parameterized (generic) capsules.
///
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_CODEGENGENERIC_HDR_GUARD
#define COMMA_TYPECHECK_CODEGENGENERIC_HDR_GUARD

#include "CodeGenCapsule.h"

namespace comma {

class CodeGenGeneric {

public:
    /// Constructs a generic pass object using the given capsule generator.
    /// This generator is asserted to wrap a parameterized capsule.
    CodeGenGeneric(CodeGenCapsule &CGC);

    /// Scans the associated capsule for references to external domains.
    ///
    /// For each external capsule found, CodeGenCapsule::addCapsuleDependency is
    /// called to register the external reference.
    void analyzeDependants();

private:
    CodeGenCapsule &CGC;

    /// The generic capsule provided by CGC.
    FunctorDecl *capsule;
};

} // end comma namespace.

#endif
