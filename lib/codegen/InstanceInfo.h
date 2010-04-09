//===-- codegen/InstanceInfo.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
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
    //@{
    /// Returns the capsule underlying this particular instance.
    const CapsuleDecl *getDefinition() const {
        return instance->getDefinition();
    }
    CapsuleDecl *getDefinition() { return instance->getDefinition(); }
    //@}

    //@{
    /// Returns the instance object corresponding to this info.
    const CapsuleInstance *getInstance() const { return instance; }
    CapsuleInstance *getInstance() { return instance; }
    //@}

    //@{
    /// Returns the domain instance declaration node this info represents, or
    /// null if this instance represents a package.
    const DomainInstanceDecl *getDomainInstanceDecl() const {
        return instance->asDomainInstance();
    }
    DomainInstanceDecl *getDomainInstanceDecl() {
        return instance->asDomainInstance();
    }
    //@}

    //@{
    /// Returns the package instance declaration node this info represents, or
    /// null if this instance represents a domain.
    const PkgInstanceDecl *getPkgInstanceDecl() const {
        return instance->asPkgInstance();
    }
    PkgInstanceDecl *getPkgInstanceDecl() {
        return instance->asPkgInstance();
    }
    //@}

    /// Returns true if this instance info corresponds to a domain.
    bool denotesDomainInstance() const {
        return instance->denotesDomainInstance();
    }

    /// Returns true if this instance info corresponds to a package.
    bool denotesPkgInstance() const {
        return instance->denotesPkgInstance();
    }

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
    /// Creates an InstanceInfo object for the given instance.
    InstanceInfo(CodeGen &CG, CapsuleInstance *instance);

    friend class CodeGen;

    /// The instance declaration associated with this info.
    CapsuleInstance *instance;

    /// Map from subroutine declarations to the corresponding SRInfo objects.
    typedef llvm::DenseMap<SubroutineDecl*, SRInfo*> SRInfoMap;
    SRInfoMap srInfoTable;

    /// The mangled name of this instance;
    std::string linkName;

    bool compiledFlag;          ///< True if this instance has been codegen'ed.

    /// The AST provides several views of a subroutine.  This routine chooses a
    /// canonical declaration accessable from all views to be used as a key in
    /// the srInfoTable.
    static SubroutineDecl *getKeySRDecl(SubroutineDecl *srDecl);

    /// Populates the srInfoTable with the declarations provided by the
    /// given instance.
    void populateInfoTable(CodeGen &CG, CodeGenTypes &CGT,
                           CapsuleInstance *instance);
};

} // end comma namespace.

#endif
