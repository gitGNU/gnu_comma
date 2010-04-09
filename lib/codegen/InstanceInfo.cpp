//===-- codegen/InstanceInfo.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGen.h"
#include "CodeGenTypes.h"
#include "InstanceInfo.h"
#include "SRInfo.h"
#include "comma/codegen/Mangle.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

SubroutineDecl *InstanceInfo::getKeySRDecl(SubroutineDecl *srDecl)
{
    // Declaration nodes which correspond to the public view always have their
    // origin link set to the corresponding private view.
    DeclRegion *region = srDecl->getDeclRegion();
    if (isa<DomainInstanceDecl>(region) || isa<PkgInstanceDecl>(region)) {
        srDecl = srDecl->getOrigin();
        region = srDecl->getDeclRegion();
    }

    // Check that some basic assumptions hold.  The declarative region must have
    // been resolved to either a PercentDecl, AddDecl, or PackageDecl.
    assert((isa<PercentDecl>(region) || isa<AddDecl>(region) ||
            isa<PackageDecl>(region))
           && "Inconsistent context for subroutine declaration!");

    // Further reduce the SubroutineDecl to its completion, if present.
    if (srDecl->getDefiningDeclaration())
        srDecl = srDecl->getDefiningDeclaration();

    return srDecl;
}

InstanceInfo::InstanceInfo(CodeGen &CG, CapsuleInstance *instance)
        : instance(instance),
          linkName(mangle::getLinkName(instance)),
          compiledFlag(false)
{
    CodeGenTypes CGT(CG, instance);

    // Populate the info table with all declarations provided by the instance.
    populateInfoTable(CG, CGT, instance);
}

void InstanceInfo::populateInfoTable(CodeGen &CG, CodeGenTypes &CGT,
                                     CapsuleInstance *instance)
{
    DeclRegion::DeclIter I;
    DeclRegion::DeclIter E;
    DeclRegion *region = instance->asDeclRegion();

    // Construct an SRInfo node for each public declaration provided by the
    // region.
    E = region->endDecls();
    for (I = region->beginDecls(); I != E; ++I) {
        /// FIXME: Support all declaration kinds.
        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*I)) {
            SubroutineDecl *key = getKeySRDecl(srDecl);
            assert(!srInfoTable.count(key) &&
                   "Multiple declarations map to the same key!");

            llvm::Function *fn = CG.makeFunction(instance, srDecl, CGT);
            srInfoTable[key] = new SRInfo(key, fn);
        }
    }

    // Similarly for all private declarations.
    AddDecl *impl;

    if (instance->denotesDomainInstance())
        impl = instance->getDefiningDomoid()->getImplementation();
    else
        impl = instance->getDefiningPackage()->getImplementation();

    E = impl->endDecls();
    for (I = impl->beginDecls(); I != E; ++I) {
        /// FIXME: Support all declaration kinds.
        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*I)) {
            SubroutineDecl *key = getKeySRDecl(srDecl);

            // If an info structure already exists for this declaration skip it.
            if(srInfoTable.count(key))
                continue;

            llvm::Function *fn = CG.makeFunction(instance, srDecl, CGT);
            srInfoTable[key] = new SRInfo(key, fn);
        }
    }
}

