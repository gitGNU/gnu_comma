//===-- codegen.InstanceInfo.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "InstanceInfo.h"
#include "SRInfo.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenTypes.h"
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
    if (isa<DomainInstanceDecl>(region)) {
        srDecl = srDecl->getOrigin();
        region = srDecl->getDeclRegion();
    }

    // Check that some basic assumptions hold.  The declarative region must have
    // been resolved to either a PercentDecl or AddDecl.
    assert((isa<PercentDecl>(region) || isa<AddDecl>(region)) &&
           "Inconsistent context for subroutine declaration!");

    // Further reduce the SubroutineDecl to its completion, if present.
    if (srDecl->getDefiningDeclaration())
        srDecl = srDecl->getDefiningDeclaration();

    return srDecl;
}

InstanceInfo::InstanceInfo(CodeGen &CG, DomainInstanceDecl *instance)
        : instance(instance),
          linkName(mangle::getLinkName(instance)),
          compiledFlag(false)
{
    CodeGenTypes CGT(CG, instance);

    DeclRegion::DeclIter I;
    DeclRegion::DeclIter E;

    // Construct an SRInfo node for each public declaration provided by the
    // instance.
    E = instance->endDecls();
    for (I = instance->beginDecls(); I != E; ++I) {
        /// FIXME: Support all declaration kinds.
        SubroutineDecl *srDecl = cast<SubroutineDecl>(*I);
        SubroutineDecl *key = getKeySRDecl(srDecl);

        assert(!srInfoTable.count(key) &&
               "Multiple declarations map to the same key!");

        llvm::Function *fn = CG.makeFunction(instance, srDecl, CGT);
        srInfoTable[key] = new SRInfo(key, fn);
    }

    // Similarly for every private declaration that is not visible thru the
    // public interface.
    Domoid *domoid = instance->getDefinition();
    AddDecl *impl = domoid->getImplementation();
    E = impl->endDecls();
    for (I = impl->beginDecls(); I != E; ++I) {
        // FIXME: Support all declaration kinds.
        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*I)) {
            SubroutineDecl *key = getKeySRDecl(srDecl);

            // If an info structure already exists for this declaration, skip
            // it.
            if (srInfoTable.count(key))
                continue;

            llvm::Function *fn = CG.makeFunction(instance, srDecl, CGT);
            srInfoTable[key] = new SRInfo(key, fn);
        }
    }
}
