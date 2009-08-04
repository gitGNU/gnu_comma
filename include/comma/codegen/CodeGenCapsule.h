//===-- codegen/CodeGenCapsule.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENCAPSULE_HDR_GUARD
#define COMMA_CODEGEN_CODEGENCAPSULE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "llvm/ADT/UniqueVector.h"

namespace comma {

class CodeGen;

class CodeGenCapsule {

private:
    typedef llvm::UniqueVector<DomainInstanceDecl *> InstanceList;

public:
    CodeGenCapsule(CodeGen &CG, Domoid *domoid);

    /// Returns the CodeGen object giving context to this generator.
    CodeGen &getCodeGen() { return CG; }
    const CodeGen &getCodeGen() const { return CG; }

    /// Returns the capsule underlying this code generator.
    Domoid *getCapsule() { return capsule; }
    const Domoid *getCapsule() const { return capsule; }

    /// Returns the link (mangled) name of the capsule.
    const std::string &getLinkName() const { return linkName; }

    /// Notifies this code generator that the underlying capsule relys on the
    /// given domain instance.  Returns a unique ID > 0 representing the
    /// instance.
    unsigned addCapsuleDependency(DomainInstanceDecl *instance);

    /// Returns the number of capsule dependencies registered.
    unsigned dependencyCount() const { return requiredInstances.size(); }

    /// Returns the instance with the given ID.
    DomainInstanceDecl *getDependency(unsigned ID) {
        return requiredInstances[ID];
    }

    /// Returns the ID of the given instance if present in the dependency set,
    /// else 0.
    unsigned getDependencyID(DomainInstanceDecl *instance) {
        return requiredInstances.idFor(instance);
    }

private:
    /// The CodeGen object used to generate this capsule.
    CodeGen &CG;

    /// The current capsule being generated.
    Domoid *capsule;

    /// The link name of the capsule;
    std::string linkName;

    /// A list of domain instances on which this capsule depends.
    llvm::UniqueVector<DomainInstanceDecl *> requiredInstances;

    /// Generate code for the given capsule.
    void emit();
};

} // end comma namespace.

#endif
