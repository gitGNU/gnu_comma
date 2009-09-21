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
        Name,        ///< Domain name              : i8*
        Ctor,        ///< Constructor function     : void (domain_instance_t)*
        ITable,      ///< Instance table           : opaque*
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
    static std::string getLinkName(const CodeGenCapsule &CGC);

    /// \brief Returns the link (mangled) name of the domain_info object that
    /// would be associated with the given capsule.
    static std::string getLinkName(const Domoid *model);

    template <FieldId F>
    struct FieldIdTraits {
        typedef const llvm::PointerType FieldType;
    };

    template <FieldId F>
    typename FieldIdTraits<F>::FieldType *getFieldType() const;

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

    /// Generates a constructor function for an instance.
    llvm::Constant *genConstructor(CodeGenCapsule &CGC);

    /// Generates a pointer to the instance table for an instance.
    llvm::Constant *genITable(CodeGenCapsule &CGC);

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

//===----------------------------------------------------------------------===//
// DomainInfo::getFieldType specializations for each field in a domain_info.

template <> inline
DomainInfo::FieldIdTraits<DomainInfo::Arity>::FieldType *
DomainInfo::getFieldType<DomainInfo::Arity>() const {
    typedef FieldIdTraits<Arity>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Arity));
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

}; // end comma namespace.

#endif
