//===-- codegen/CGContext.cpp --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CGContext.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CGContext::CGContext(CodeGen &CG, InstanceInfo *IInfo)
    : CG(CG),
      IInfo(IInfo),
      CGT(CG, IInfo->getInstanceDecl())
{
    DomainInstanceDecl *instance = IInfo->getInstanceDecl();
    Domoid *domoid = IInfo->getDefinition();
    if (FunctorDecl *functor = dyn_cast<FunctorDecl>(domoid)) {
        for (unsigned i = 0; i < instance->getArity(); ++i) {
            DomainType *formal = functor->getFormalType(i);
            DomainType *actual = instance->getActualParamType(i);
            paramMap[formal] = actual;
        }
    }
}

bool CGContext::generatingCapsuleInstance() const
{
    if (generatingCapsule())
        return IInfo->getInstanceDecl()->isParameterized();
    return false;
}

DomainInstanceDecl *
CGContext::rewriteAbstractDecl(AbstractDomainDecl *abstract) const {
    typedef ParameterMap::const_iterator iterator;
    iterator I = paramMap.find(abstract->getType());
    if (I == paramMap.end())
        return 0;
    DomainType *domTy = cast<DomainType>(I->second);
    return cast<DomainInstanceDecl>(domTy->getDomainTypeDecl());
}


