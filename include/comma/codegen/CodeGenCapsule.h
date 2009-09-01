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
    CodeGenCapsule(CodeGen &CG, DomainDecl *domain);
    CodeGenCapsule(CodeGen &CG, FunctorDecl *functor);
    CodeGenCapsule(CodeGen &CG, DomainInstanceDecl *instance);

    /// Generate code for the given capsule.
    void emit();

    /// Returns true if we are currently generating an instance.
    ///
    /// More precisely: The context of the compilation is a parameterized
    /// capsule for which we are generating specialized code for a particular
    /// concrete parameterization, or the current capsule is not parameterized.
    /// This method is the inverse of generatingGeneric.
    bool generatingInstance() const { return theInstance != 0; }

    /// Returns ture if we are generating a parameterized instance.
    bool generatingParameterizedInstance() const;

    /// Returns true if we are currently generating a generic.
    ///
    /// This name is something of a misnomer.  We do not generate shared code
    /// for generics (parameterized capsules) except for a constructor function
    /// used to build the runtime representation of instances.  This method is
    /// the inverse of generatingInstance.
    bool generatingGeneric() const { return theInstance == 0; }

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

    /// Returns the CodeGen object giving context to this generator.
    CodeGen &getCodeGen() { return CG; }
    const CodeGen &getCodeGen() const { return CG; }

    /// Returns the capsule underlying this code generator.
    Domoid *getCapsule() { return capsule; }
    const Domoid *getCapsule() const { return capsule; }

    /// Returns the link (mangled) name of the capsule.
    std::string getLinkName() const;

    /// \brief Returns the name of the given subroutine as it should appear in
    /// LLVM IR.
    ///
    /// The conventions followed by Comma model those of Ada.  In particular, a
    /// subroutines link name is similar to its fully qualified name, except
    /// that the double colon is replaced by an underscore, and overloaded names
    /// are identified using a postfix number.  For example, the Comma name
    /// "D::Foo" is translated into "D__Foo" and subsequent overloads are
    /// translated into "D__Foo__1", "D__Foo__2", etc.  Operator names like "*"
    /// or "+" are given names beginning with a zero followed by a spelled out
    /// alternative.  For example, "*" translates into "0multiply" and "+"
    /// translates into "0plus" (with appropriate qualification prefix and
    /// overload suffix).
    std::string getLinkName(const SubroutineDecl *sr) const;

    std::string getLinkName(const DomainInstanceDecl *instance,
                            const SubroutineDecl *sr) const;

    /// \brief Returns the name of the given Domoid as it should appear in LLVM
    /// IR.
    std::string getLinkName(const Domoid *domoid) const;

    /// \brief Returns the name of the given domain instance as it should appear
    /// in LLVM IR.
    std::string getLinkName(const DomainInstanceDecl *instance) const;

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

    /// The current capsule being generated.
    Domoid *capsule;

    /// If we are generating an instance, this member points to it.  Otherwise
    /// it is null.
    DomainInstanceDecl *theInstance;

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

    /// \brief Returns the name of a subroutine, translating binary operators
    /// into a unique long-form.
    ///
    /// For example, "*" translates into "0multiply" and "+" translates into
    /// "0plus".  This is a helper function for CodeGen::getLinkName.
    static std::string getSubroutineName(const SubroutineDecl *srd);
};

} // end comma namespace.

#endif
