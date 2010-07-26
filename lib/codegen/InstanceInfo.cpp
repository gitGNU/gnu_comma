//===-- codegen/InstanceInfo.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

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
    if (isa<PkgInstanceDecl>(region)) {
        srDecl = srDecl->getOrigin();
        region = srDecl->getDeclRegion();
    }

    // Check that some basic assumptions hold.  The declarative region must have
    // been resolved to either a BodyDecl, PrivatePart, or PackageDecl.
    assert((isa<BodyDecl>(region) || isa<PackageDecl>(region) || 
            isa<PrivatePart>(region))
           && "Inconsistent context for subroutine declaration!");

    // Further reduce the SubroutineDecl to its completion, if present.
    if (srDecl->getDefiningDeclaration())
        srDecl = srDecl->getDefiningDeclaration();

    return srDecl;
}

InstanceInfo::InstanceInfo(CodeGen &CG, PkgInstanceDecl *instance)
        : instance(instance),
          linkName(mangle::getLinkName(instance)),
          compiledFlag(false)
{
    // Populate the info table with all declarations provided by the instance.
    populateInfoTable(CG, CG.getCGT(), instance);
}

void InstanceInfo::populateInfoTable(CodeGen &CG, CodeGenTypes &CGT,
                                     PkgInstanceDecl *instance)
{
    DeclRegion::DeclIter I;
    DeclRegion::DeclIter E;

    // Construct an SRInfo node for each public declaration provided by the
    // region.
    E = instance->endDecls();
    for (I = instance->beginDecls(); I != E; ++I) {
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
    BodyDecl *impl = instance->getDefinition()->getImplementation();

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

