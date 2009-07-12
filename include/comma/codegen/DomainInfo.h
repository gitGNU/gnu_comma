//===-- codegen/DomainInfo.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_DOMAININFO_HDR_GUARD
#define COMMA_CODEGEN_DOMAININFO_HDR_GUARD

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"

#include <string>
#include <vector>

namespace comma {

/// The DomainInfo class generates the static runtime tables used to represent
/// domains.
class DomainInfo {

public:
    DomainInfo(CodeGen *CG, const DomainDecl *domain)
        : CG(CG), domoid(domain) { emit(); }

    DomainInfo(CodeGen *CG, const FunctorDecl *functor)
        : CG(CG), domoid(functor) { emit(); }

    /// Returns the domain associated with this DomainInfo.
    const Domoid *getDomoid() const { return domoid; }

    std::vector<unsigned> offsets;

private:
    CodeGen *CG;
    const Domoid *domoid;

    /// The name of the domain.
    llvm::Constant *infoName;

    /// The arity of the domain.
    llvm::Constant *infoArity;

    /// Generates the IR name for the domain info structure.
    const std::string getDomainInfoName() const;

    /// Emits the domain name, populating infoName.
    void emitInfoName();

    /// Emits the domains arity, populating infoArity.
    void emitInfoArity();

    /// Populates the offset vector with the offsets for the given signature
    /// (and super signatures) begining with the provided offset index.  The
    /// adjusted offset index is returned.
    unsigned emitOffsetsForSignature(const SignatureType *sig, unsigned offset);

    /// Populates the offset vector.
    void emitSignatureOffsets();

    /// Emits the table of the associated domain into the given module.
    void emit();
};

}; // end comma namespace.

#endif
