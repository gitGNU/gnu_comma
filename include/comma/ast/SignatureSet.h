//===-- ast/SignatureSet.h ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// The following class provides a container for the set of signatures associated
// with a particular model.  Note that is class is not a member of the Ast
// hierarchy.
//
// SignatureSet's are populated by specifying the direct signatures (those
// signatures which are explicitly named in the declaration of the associated
// model).  Recursively, the SignatureSet's associated with the direct
// signatures are then consulted to obtain the set of indirect signatures.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_SIGNATURESET_HDR_GUARD
#define COMMA_AST_SIGNATURESET_HDR_GUARD

#include "llvm/ADT/SmallPtrSet.h"

namespace comma {

class SignatureType;
class Model;

class SignatureSet {

    typedef llvm::SmallPtrSet<SignatureType*, 8> SignatureTable;

    SignatureTable directSignatures;
    SignatureTable allSignatures;

    ModelDecl *associatedModel;

public:
    SignatureSet(ModelDecl *model)
        : associatedModel(model) { }

    // \brief Adds a direct signature to this set.
    //
    // Returns true if the signature did not previously exist in this set and
    // false otherwise.
    bool addDirectSignature(SignatureType *signature);

    /// \brief Returns true is the given signature type is already contained in
    /// this set.
    bool contains(SignatureType *signature) const {
        return allSignatures.count(signature);
    }

    /// \brief Returns true if the given signature type is a direct signature of
    /// this set.
    bool isDirect(SignatureType *signature) const {
        return directSignatures.count(signature);
    }

    /// \brief Returns true if the given signature type is an indirect signature
    /// of this set.
    bool isIndirect(SignatureType *signature) const {
        return contains(signature) && !isDirect(signature);
    }

    /// \brief Returns the number direct and indirect signatures in this set.
    unsigned numSignatures() const { return allSignatures.size(); }

    /// \brief Analogus to numSignatures().
    unsigned size() const { return numSignatures(); }

    /// \brief Returns the number of direct signatures in this set.
    unsigned numDirectSignatures() const { return directSignatures.size(); }

    /// \brief Returns the number of indirect signatures in this set.
    unsigned numIndirectSignatures() const {
        return numSignatures() - numDirectSignatures();
    }

    typedef SignatureTable::iterator iterator;
    typedef SignatureTable::const_iterator const_iterator;

    iterator beginDirect() { return directSignatures.begin(); }
    iterator endDirect()   { return directSignatures.end(); }

    const_iterator beginDirect() const { return directSignatures.begin(); }
    const_iterator endDirect()   const { return directSignatures.end(); }

    iterator begin() { return allSignatures.begin(); }
    iterator end()   { return allSignatures.end(); }

    const_iterator begin() const { return allSignatures.begin(); }
    const_iterator end()   const { return allSignatures.end(); }

    const ModelDecl *getAssociatedModel() const { return associatedModel; }
    ModelDecl *getAssociatedModel() { return associatedModel; }
};

} // End comma namespace

#endif
