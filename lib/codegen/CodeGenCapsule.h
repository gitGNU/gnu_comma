//===-- codegen/CodeGenCapsule.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENCAPSULE_HDR_GUARD
#define COMMA_CODEGEN_CODEGENCAPSULE_HDR_GUARD

#include "InstanceInfo.h"
#include "comma/ast/AstBase.h"
#include "comma/codegen/CodeGenTypes.h"

#include "llvm/ADT/UniqueVector.h"
#include "llvm/ADT/SmallVector.h"

#include <map>

namespace comma {

class CodeGen;

class CodeGenCapsule {

private:
    typedef llvm::UniqueVector<DomainInstanceDecl *> InstanceList;

public:
    CodeGenCapsule(CodeGen &CG, InstanceInfo *instance);
    CodeGenCapsule(CodeGen &CG, FunctorDecl *functor);

    /// Generate code for the given capsule.
    void emit();

    /// Returns true if we are currently generating an instance.
    ///
    /// More precisely: The context of the compilation is a parameterized
    /// capsule for which we are generating specialized code for a particular
    /// concrete parameterization, or the current capsule is not parameterized.
    /// This method is the inverse of generatingGeneric.
    bool generatingInstance() const { return theInstanceInfo != 0; }

    /// Returns ture if we are generating a parameterized instance.
    bool generatingParameterizedInstance() const;

    /// Returns true if we are currently generating a generic.
    ///
    /// This name is something of a misnomer.  We do not generate shared code
    /// for generics (parameterized capsules) except for a constructor function
    /// used to build the runtime representation of instances.  This method is
    /// the inverse of generatingInstance.
    bool generatingGeneric() const { return theInstanceInfo == 0; }

    /// Returns the instance being compiled.
    ///
    /// For a non-parameterized capsule, this method returns the unique instance
    /// declaration representing the public view of the capsule.  Similarly, for
    /// a parameterized capsule, the instance returned is a non-dependent
    /// instance.
    ///
    /// This method will assert when generatingGeneric returns true.
    DomainInstanceDecl *getInstance();
    const DomainInstanceDecl *getInstance() const;

    /// Returns the type generator used for building types within this capsules
    /// context.
    CodeGenTypes &getTypeGenerator() { return CGT; }
    const CodeGenTypes &getTypeGenerator() const { return CGT; }

    /// Returns the CodeGen object giving context to this generator.
    CodeGen &getCodeGen() { return CG; }
    const CodeGen &getCodeGen() const { return CG; }

    /// Returns the capsule underlying this code generator.
    Domoid *getCapsule() { return capsule; }
    const Domoid *getCapsule() const { return capsule; }

    /// Returns the link (mangled) name of the associated capsule instance.
    const std::string &getLinkName() const { return capsuleLinkName; }

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

    typedef std::map<Type*, Type*> ParameterMap;
    const ParameterMap &getParameterMap() const { return paramMap; }

    /// Returns the instance decl corresponding to the given abstract decl using
    /// the current parameter map, or null if there is no association.
    DomainInstanceDecl *rewriteAbstractDecl(AbstractDomainDecl *abstract) const;

private:
    /// The CodeGen object used to generate this capsule.
    CodeGen &CG;

    /// A type generator for this capsules instance.
    CodeGenTypes CGT;

    /// The current capsule being generated.
    Domoid *capsule;

    /// The link name of the capsule.
    std::string capsuleLinkName;

    /// If we are generating an instance, this member points to its info node.
    /// Otherwise it is null.
    InstanceInfo *theInstanceInfo;

    /// A map from the formal parameters of the current capsule to the actual
    /// parameters (non-empty only when we are generating code for a
    /// parameterized instance).
    ParameterMap paramMap;

    /// A list of domain instances on which this capsule depends.
    llvm::UniqueVector<DomainInstanceDecl *> requiredInstances;

    /// \brief Returns the index of a decl within a declarative region.
    ///
    /// This function scans the given region for the given decl.  For each
    /// overloaded name matching that of the decl, the index returned is
    /// incremented (and since DeclRegion's maintain declaration order, the
    /// index represents the zeroth, first, second, ..., declaration of the
    /// given name).  If no matching declaration is found in the region, -1 is
    /// returned.
    static int getDeclIndex(const Decl *decl, const DeclRegion *region);
};

} // end comma namespace.

#endif
