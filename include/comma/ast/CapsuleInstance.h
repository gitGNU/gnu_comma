//===-- ast/CapsuleInstance.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_CAPSULEINSTANCE_HDR_GUARD
#define COMMA_AST_CAPSULEINSTANCE_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

/// \class
/// \brief Common base for domain and package instance nodes.
class CapsuleInstance {

public:
    virtual ~CapsuleInstance() { }

    /// Returns true if this is a DomainInstanceDecl.
    bool denotesDomainInstance() const { return asDomainInstance() != 0; }

    /// Returns true if this is a PkgInstanceDecl.
    bool denotesPkgInstance() const { return asPkgInstance() != 0; }

    //@{
    /// Returns this cast to a DomainInstanceDecl or null.
    DomainInstanceDecl *asDomainInstance();
    const DomainInstanceDecl *asDomainInstance() const;
    //@}

    //@{
    /// Returns this case to a PkgInstanceDecl or null.
    PkgInstanceDecl *asPkgInstance();
    const PkgInstanceDecl *asPkgInstance() const;
    //@}

    //@{
    /// Converts this to a raw AST node.
    Ast *asAst();
    const Ast *asAst() const {
        return const_cast<CapsuleInstance*>(this)->asAst();
    }
    //@}

    //@{
    /// Converts this to a DeclRegion.
    DeclRegion *asDeclRegion();
    const DeclRegion *asDeclRegion() const {
        return const_cast<CapsuleInstance*>(this)->asDeclRegion();
    }
    //@}

    //@{
    /// Returns the capsule defining this instance.
    CapsuleDecl *getDefinition() { return Definition; }
    const CapsuleDecl *getDefinition() const { return Definition; }
    //@}

    //@{
    /// Returns the defining Domoid if this is a domain instance.
    Domoid *getDefiningDomoid();
    const Domoid *getDefiningDomoid() const {
        return const_cast<CapsuleInstance*>(this)->getDefiningDomoid();
    }
    //@}

    //@{
    /// If this is a domain instance return the corresponding domain
    /// declaration.  Otherwise null is returned.
    DomainDecl *getDefiningDomain();
    const DomainDecl *getDefiningDomain() const {
        return const_cast<CapsuleInstance*>(this)->getDefiningDomain();
    }
    //@}

    //@{
    /// If this is a functor instance, return the corresponding functor
    /// declaration.  Otherwise null is returned.
    FunctorDecl *getDefiningFunctor();
    const FunctorDecl *getDefiningFunctor() const {
        return const_cast<CapsuleInstance*>(this)->getDefiningFunctor();
    }
    //@}

    //@{
    /// If this is a package instance, return the corresponding package
    /// declaration.  Otherwise null is returned.
    PackageDecl *getDefiningPackage();
    const PackageDecl *getDefiningPackage() const {
        return const_cast<CapsuleInstance*>(this)->getDefiningPackage();
    }
    //@}

    /// Returns true if this instance represents percent or is a parameterized
    /// instance, and in the latter case, if any of the arguments involve
    /// generic formal parameters or percent nodes.
    bool isDependent() const;

    /// Returns the arity of the underlying declaration.
    unsigned getArity() const;

    /// Returns true if this is an instance of a functor.
    bool isParameterized() const { return getArity() != 0; }

    /// Returns the i'th actual parameter.  This method asserts if its argument
    /// is out of range, or if this is not a parameterized instance.
    DomainTypeDecl *getActualParam(unsigned n) const {
        assert(isParameterized() && "Not a parameterized instance!");
        assert(n < getArity() && "Index out of range!");
        return Arguments[n];
    }

    //@{
    /// \brief Returns the type of the i'th actual parameter.
    ///
    /// This method asserts if its argument is out of range, or if this is not
    /// an instance of a functor.
    DomainType *getActualParamType(unsigned n);
    const DomainType *getActualParamType(unsigned n) const {
        return const_cast<CapsuleInstance*>(this)->getActualParamType(n);
    }

    //@}

    /// Iterators over the arguments supplied to this instance.
    typedef DomainTypeDecl **arg_iterator;
    arg_iterator beginArguments() const { return Arguments; }
    arg_iterator endArguments() const { return &Arguments[getArity()]; }

    // Support isa and dyn_cast.
    static bool classof(const Ast *node) {
        Ast::AstKind kind = node->getKind();
        return (kind == Ast::AST_DomainInstanceDecl ||
                kind == Ast::AST_PkgInstanceDecl);
    }
    static bool classof(const DomainInstanceDecl *node) { return true; }
    static bool classof(const PkgInstanceDecl *node) { return true; }

protected:
    /// Constructs a CapsuleInstance corresponding to a package.
    CapsuleInstance(PackageDecl *definintion);

    /// Constructs a CapsuleInstance corresponding to a domain.
    CapsuleInstance(DomainDecl *definition);

    /// Constructs a CapsuleInstance corresponding to a functor.
    CapsuleInstance(FunctorDecl *definition,
                    DomainTypeDecl **args, unsigned numArgs);

    CapsuleDecl *Definition;
    DomainTypeDecl **Arguments;
};

} // end comma namespace

namespace llvm {

// Specialize isa_impl_wrap to test if a CapsuleInstance is a specific Ast node.
template<class To>
struct isa_impl_wrap<To,
                     const comma::CapsuleInstance, const comma::CapsuleInstance> {
    static bool doit(const comma::CapsuleInstance &val) {
        return To::classof(val.asAst());
    }
};

template<class To>
struct isa_impl_wrap<To, comma::CapsuleInstance, comma::CapsuleInstance>
  : public isa_impl_wrap<To,
                         const comma::CapsuleInstance,
                         const comma::CapsuleInstance> { };

// Ast to CapsuleInstance conversions.
template<class From>
struct cast_convert_val<comma::CapsuleInstance, From, From> {
    static comma::CapsuleInstance &doit(const From &val) {
        const From *ptr = &val;
        return (dyn_cast<comma::DomainInstanceDecl>(ptr) ||
                dyn_cast<comma::PkgInstanceDecl>(ptr));
    }
};

template<class From>
struct cast_convert_val<comma::CapsuleInstance, From*, From*> {
    static comma::CapsuleInstance *doit(const From *val) {
        return (dyn_cast<comma::DomainInstanceDecl>(val) ||
                dyn_cast<comma::PkgInstanceDecl>(val));
    }
};

template<class From>
struct cast_convert_val<const comma::CapsuleInstance, From, From> {
    static const comma::CapsuleInstance &doit(const From &val) {
        const From *ptr = &val;
        return (dyn_cast<comma::DomainInstanceDecl>(ptr) ||
                dyn_cast<comma::PkgInstanceDecl>(ptr));
    }
};

template<class From>
struct cast_convert_val<const comma::CapsuleInstance, From*, From*> {
    static const comma::CapsuleInstance *doit(const From *val) {
        return (dyn_cast<comma::DomainInstanceDecl>(val) ||
                dyn_cast<comma::PkgInstanceDecl>(val));
    }
};

// CapsuleInstance to Ast conversions.
template<class To>
struct cast_convert_val<To,
                        const comma::CapsuleInstance,
                        const comma::CapsuleInstance> {
    static To &doit(const comma::CapsuleInstance &val) {
        return *reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val.asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::CapsuleInstance, comma::CapsuleInstance>
    : public cast_convert_val<To,
                              const comma::CapsuleInstance,
                              const comma::CapsuleInstance> { };

template<class To>
struct cast_convert_val<To,
                        const comma::CapsuleInstance*,
                        const comma::CapsuleInstance*> {
    static To *doit(const comma::CapsuleInstance *val) {
        return reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val->asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::CapsuleInstance*, comma::CapsuleInstance*>
    : public cast_convert_val<To,
                              const comma::CapsuleInstance*,
                              const comma::CapsuleInstance*> { };

} // end llvm namespace

#endif
