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

#include "llvm/Support/IRBuilder.h"

#include <string>
#include <vector>

namespace comma {

class CommaRT;

/// The DomainInfo class generates the static runtime tables used to represent
/// domains.
class DomainInfo {

public:
    DomainInfo(CommaRT &CRT);

    void init();

    /// Each member of FieldID represents a component of a domain_info structure
    /// type.  For use as named keys when accessing and setting domain_info
    /// fields.
    ///
    /// NOTE: The enumeration ordering must match that of the actual structure.
    enum FieldId {
        Arity,       ///< Domain arity             : i32
        NumSigs,     ///< Number of signatures     : i32
        Name,        ///< Domain name              : i8*
        Ctor,        ///< Constructor function     : void (domain_instance_t)*
        ITable,      ///< Instance table           : opaque*
        SigOffsets,  ///< Signature offset vector  : i32*
        Exvec        ///< Export vector            : void ()**
    };

    /// Returns the name of a domain_info type.
    const std::string &getTypeName() const { return theTypeName; }

    /// Returns a structure type describing a domain_info object.
    const llvm::StructType *getType() const;

    /// Returns a pointer-to domain_info structure type.
    const llvm::PointerType *getPointerTypeTo() const;

    /// Returns a pointer-to function type describing a domain constructor
    /// function.
    ///
    /// A domain constructor is a pointer-to function type of the form
    /// \code{void (domain_instance_t)*}.
    const llvm::PointerType *getCtorPtrType() const;

    /// \brief Creates an instance of a domain_info object.
    ///
    /// A non-constant global variable is generated, initialized, and injected
    /// into the current module.
    llvm::GlobalVariable *generateInstance(CodeGenCapsule &CGC);

    /// \brief Returns the link (mangled) name of the domain_info object
    /// that would be associated with the capsule given by \p CGC.
    std::string getLinkName(const CodeGenCapsule &CGC) const;

    template <FieldId F>
    struct FieldIdTraits {
        typedef const llvm::PointerType FieldType;
    };

    template <FieldId F>
    typename FieldIdTraits<F>::FieldType *getFieldType() const;

    /// Loads an offset at the given index from a domain_info's signature offset
    /// vector.
    llvm::Value *indexSigOffset(llvm::IRBuilder<> &builder,
                                llvm::Value *DInfo,
                                llvm::Value *index) const;

    /// Loads a pointer to the first element of the export vector associated
    /// with the given domain_info object.
    llvm::Value *loadExportVec(llvm::IRBuilder<> &builder,
                               llvm::Value *DInfo) const;

    /// Indexes into the export vector and loads a pointer to the associated
    /// export.
    llvm::Value *loadExportFn(llvm::IRBuilder<> &builder,
                              llvm::Value *DInfo,
                              llvm::Value *exportIdx) const;

private:
    CommaRT &CRT;
    CodeGen &CG;
    const llvm::TargetData &TD;

    /// The name of this type.
    static const std::string theTypeName;

    /// The structure type describing a domain_info.
    llvm::PATypeHolder theType;

    /// Allocates a constant string for a domain_info's name.
    llvm::Constant *genName(CodeGenCapsule &CGC);

    /// Generates the arity for an instance.
    llvm::Constant *genArity(CodeGenCapsule &CGC);

    /// Generates the signature count for an instance.
    llvm::Constant *genSignatureCount(CodeGenCapsule &CGC);

    /// Generates a constructor function for an instance.
    llvm::Constant *genConstructor(CodeGenCapsule &CGC);

    /// Generates a pointer to the instance table for an instance.
    llvm::Constant *genITable(CodeGenCapsule &CGC);

    /// Generates the signature offset vector for an instance.
    llvm::Constant *genSignatureOffsets(CodeGenCapsule &CGC);

    /// Generates the export array for an instance.
    llvm::Constant *genExportArray(CodeGenCapsule &CGC);

    /// \brief Helper method for genSignatureOffsets.
    ///
    /// Populates the given vector \p offsets with constant indexes,
    /// representing the offsets required to appear in a domain_info's
    /// signature_offset field.  Each of the offsets is adjusted by \p index.
    unsigned
    getOffsetsForSignature(SignatureType *sig,
                           unsigned index,
                           std::vector<llvm::Constant *> &offsets);

    /// \brief Helper method for genConstructor.
    ///
    /// Generates a call to get_domain for the capsule dependency represented by
    /// \p ID.  The dependency (and any outstanding sub-dependents) are
    /// consecutively stored into \p destVector beginning at an index derived
    /// from \p ID.  \p percent represents the domain_instance serving as
    /// argument to the constructor.
    void genInstanceRequirement(llvm::IRBuilder<> &builder,
                                CodeGenCapsule &CGC,
                                unsigned ID,
                                llvm::Value *destVector,
                                llvm::Value *percent);

    /// \brief Helper method for genInstanceRequirement.
    ///
    /// Constructs the dependency info for the dependency represented by \p ID,
    /// which must be a non-parameterized domain.
    void genDomainRequirement(llvm::IRBuilder<> &builder,
                              CodeGenCapsule &CGC,
                              unsigned ID,
                              llvm::Value *destVector);

    /// \brief Helper method for genInstanceRequirement.
    ///
    /// Constructs the dependency info for the dependency represented by \p ID,
    /// which must be a parameterized domain (functor).
    void genFunctorRequirement(llvm::IRBuilder<> &builder,
                               CodeGenCapsule &CGC,
                               unsigned ID,
                               llvm::Value *destVector,
                               llvm::Value *percent);
};

//===----------------------------------------------------------------------===//
// DomainInfo::FieldIdTraits specializations for the various fields within a
// domain_info.

template <>
struct DomainInfo::FieldIdTraits<DomainInfo::Arity> {
    typedef const llvm::IntegerType FieldType;
};

template <>
struct DomainInfo::FieldIdTraits<DomainInfo::NumSigs> {
    typedef const llvm::IntegerType FieldType;
};

//===----------------------------------------------------------------------===//
// DomainInfo::getFieldType specializations for each field in a domain_info.

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::Arity>::FieldType *
DomainInfo::getFieldType<DomainInfo::Arity>() const {
    typedef FieldIdTraits<Arity>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Arity));
}

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::NumSigs>::FieldType *
DomainInfo::getFieldType<DomainInfo::NumSigs>() const {
    typedef FieldIdTraits<NumSigs>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(NumSigs));
}

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::Name>::FieldType *
DomainInfo::getFieldType<DomainInfo::Name>() const {
    typedef FieldIdTraits<Name>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Name));
}

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::Ctor>::FieldType *
DomainInfo::getFieldType<DomainInfo::Ctor>() const {
    typedef FieldIdTraits<Ctor>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Ctor));
}

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::ITable>::FieldType *
DomainInfo::getFieldType<DomainInfo::ITable>() const {
    typedef FieldIdTraits<ITable>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(ITable));
}

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::SigOffsets>::FieldType *
DomainInfo::getFieldType<DomainInfo::SigOffsets>() const {
    typedef FieldIdTraits<SigOffsets>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(SigOffsets));
}

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::Exvec>::FieldType *
DomainInfo::getFieldType<DomainInfo::Exvec>() const {
    typedef FieldIdTraits<Exvec>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Exvec));
}

}; // end comma namespace.

#endif
