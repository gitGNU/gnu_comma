//===-- codegen/ExportMap.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_EXPORTMAP_HDR_GUARD
#define COMMA_CODEGEN_EXPORTMAP_HDR_GUARD

#include "comma/ast/AstBase.h"

#include <map>
#include <vector>

namespace comma {

class ExportMap {

private:
    typedef std::pair<const SubroutineDecl *, unsigned> ExportPair;

    struct SignatureEntry {
        SignatureEntry() : sigoid(0) { }
        const Sigoid *sigoid;
        std::vector<unsigned> offsets;
        std::vector<ExportPair> exports;
        unsigned totalExports;
    };

public:
    typedef const SignatureEntry &SignatureKey;

    const SignatureKey addSignature(const Sigoid *sig) {
        SignatureEntry &entry = signatureTable[sig];

        // If the returned value is default constructed, there is no entry yet
        // in our table.  Initialize.
        if (entry.sigoid == 0)
            initializeUsingSignature(entry, sig);

        return entry;
    }

    unsigned getIndex(const SubroutineDecl *decl);

    const SignatureKey lookupSignature(const Sigoid *sig) {
        return addSignature(sig);
    }

    static const Sigoid *getSigoid(const SignatureKey entry) {
        return entry.sigoid;
    }

    static unsigned getSignatureOffset(const SubroutineDecl *decl);

    unsigned getLocalIndex(const SubroutineDecl *decl);

    typedef std::vector<unsigned>::const_iterator offset_iterator;

    static offset_iterator begin_offsets(const SignatureKey entry) {
        return entry.offsets.begin();
    }

    static offset_iterator end_offsets(const SignatureKey entry) {
        return entry.offsets.end();
    }

    static unsigned numberOfExports(SignatureKey key) {
        return key.totalExports;
    }

    /// Debug method printing the given key to stderr.
    void dump(const SignatureKey entry);

private:
    typedef std::map<const Sigoid *, SignatureEntry> SignatureMap;
    SignatureMap signatureTable;

    void initializeUsingSignature(SignatureEntry &entry, const Sigoid *sig);

    static const Sigoid *resolveSignatureContext(const SubroutineDecl *decl);

    static const Sigoid *resolveSignatureOrigin(const SubroutineDecl *decl);
};


} // end comma namespace.

#endif
