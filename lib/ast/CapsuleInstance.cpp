//===-- ast/CapsuleInstance.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/CapsuleInstance.h"
#include "comma/ast/Decl.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CapsuleInstance::CapsuleInstance(PackageDecl *package)
    : Definition(package),
      Arguments(0) { }

CapsuleInstance::CapsuleInstance(DomainDecl *domain)
    : Definition(domain),
      Arguments(0) { }

CapsuleInstance::CapsuleInstance(FunctorDecl *functor,
                                 DomainTypeDecl **args, unsigned numArgs)
    : Definition(functor)
{
    assert(functor->getArity() == numArgs &&
           "Wrong number of arguments for instance!");

    Arguments = new DomainTypeDecl*[numArgs];
    std::copy(args, args + numArgs, Arguments);
}

DomainInstanceDecl *CapsuleInstance::asDomainInstance()
{
    return dyn_cast<DomainInstanceDecl>(this);
}

const DomainInstanceDecl *CapsuleInstance::asDomainInstance() const
{
    return dyn_cast<DomainInstanceDecl>(this);
}

PkgInstanceDecl *CapsuleInstance::asPkgInstance()
{
    return dyn_cast<PkgInstanceDecl>(this);
}

const PkgInstanceDecl *CapsuleInstance::asPkgInstance() const
{
    return dyn_cast<PkgInstanceDecl>(this);
}

Ast *CapsuleInstance::asAst()
{
    if (isa<Domoid>(Definition))
        return static_cast<DomainInstanceDecl*>(this);
    else
        return static_cast<PkgInstanceDecl*>(this);
}

DeclRegion *CapsuleInstance::asDeclRegion()
{
    if (denotesDomainInstance())
        return asDomainInstance();
    return asPkgInstance();
}

Domoid *CapsuleInstance::getDefiningDomoid()
{
    return dyn_cast<Domoid>(Definition);
}

DomainDecl *CapsuleInstance::getDefiningDomain()
{
    return dyn_cast<DomainDecl>(Definition);
}

FunctorDecl *CapsuleInstance::getDefiningFunctor()
{
    return dyn_cast<FunctorDecl>(Definition);
}

PackageDecl *CapsuleInstance::getDefiningPackage()
{
    return dyn_cast<PackageDecl>(Definition);
}

unsigned CapsuleInstance::getArity() const
{
    // FIXME: Support parameterized packages.
    if (const FunctorDecl *functor = getDefiningFunctor())
        return functor->getArity();
    return 0;
}

bool CapsuleInstance::isDependent() const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        const DomainType *param = cast<DomainType>(getActualParamType(i));
        if (param->isAbstract())
            return true;
        if (param->denotesPercent())
            return true;
        if (param->getInstanceDecl()->isDependent())
            return true;
    }
    return false;
}

DomainType *CapsuleInstance::getActualParamType(unsigned n)
{
    return getActualParam(n)->getType();
}
