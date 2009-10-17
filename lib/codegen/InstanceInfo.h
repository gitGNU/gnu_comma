//===-- codegen/InstanceInfo.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Information associated with each capsule instance emitted by the code
/// generator.
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_INSTANCEINFO_HDR_GUARD
#define COMMA_CODEGEN_INSTANCEINFO_HDR_GUARD

#include "comma/ast/Decl.h"

#include "llvm/ADT/DenseMap.h"

namespace comma {

class CodeGen;
class SRInfo;

class InstanceInfo {

public:
    /// Returns the domoid underlying this particular instance.
    Domoid *getDefinition() { return instance->getDefinition(); }
    const Domoid *getDefinition() const { return instance->getDefinition(); }

    /// Returns the instance declaration node this info represents.
    DomainInstanceDecl *getInstanceDecl() { return instance; }
    const DomainInstanceDecl *getInstanceDecl() const { return instance; }

    /// Returns the link (mangled) name of this instance.
    llvm::StringRef getLinkName() const { return linkName; }

    /// Retrieves the SRInfo associated with the given subroutine declaration
    /// (via a previous call to addSRInfo).  Returns the object if it exists
    /// else null.
    SRInfo *lookupSRInfo(SubroutineDecl *srDecl) {
        return srInfoTable.lookup(getKeySRDecl(srDecl));
    }

    /// Like lookupSRInfo, but asserts if the lookup failed.
    SRInfo *getSRInfo(SubroutineDecl *srDecl) {
        SRInfo *info = lookupSRInfo(srDecl);
        assert(info && "SRInfo lookup failed!");
        return info;
    }

    /// Marks this instance has having been compiled.
    void markAsCompiled() { compiledFlag = true; }

    /// Returns true if code for this instance has been emitted.
    bool isCompiled() const { return compiledFlag; }

private:
    /// Creates an InstanceInfo object for the given domain instance.
    InstanceInfo(CodeGen &CG, DomainInstanceDecl *instance);
    friend class CodeGen;

    /// The domain instance associated with this info.
    DomainInstanceDecl *instance;

    /// Map from subroutine declarations to the corresponding SRInfo objects.
    typedef llvm::DenseMap<SubroutineDecl*, SRInfo*> SRInfoMap;
    SRInfoMap srInfoTable;

    /// The mangled name of this instance;
    std::string linkName;

    bool compiledFlag;            ///< True if this instance has been codegen'ed.

    /// The AST provides several views of a subroutine.  This routine chooses a
    /// canonical declaration accessable from all views to be used as a key in
    /// the srInfoTable.
    static SubroutineDecl *getKeySRDecl(SubroutineDecl *srDecl);
};

} // end comma namespace.

#endif
