//===-- codegen/DomainInstance.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_DOMAININSTANCE_HDR_GUARD
#define COMMA_CODEGEN_DOMAININSTANCE_HDR_GUARD

#include "comma/codegen/CodeGen.h"
#include "llvm/Support/IRBuilder.h"

namespace comma {

class CommaRT;

class DomainInstance {

public:
    DomainInstance(CommaRT &CRT);

    void init();

    enum FieldId {
        Info,
        Next,
        Params,
        Views,
        Requirements
    };

    /// Returns the name of a domain_instance type.
    const std::string &getTypeName() const { return theTypeName; }

    /// Returns a structure type describing a domain_instance object.
    const llvm::StructType *getType() const;

    /// Returns a pointer-to domain_instance structure type.
    const llvm::PointerType *getPointerTypeTo() const;

    template <FieldId F>
    struct FieldIdTraits {
        typedef const llvm::PointerType FieldType;
    };

    template <FieldId F>
    typename FieldIdTraits<F>::FieldType *getFieldType() const;

    /// Loads the domain_info associated with the given domain_instance.
    llvm::Value *loadInfo(llvm::IRBuilder<> &builder,
                          llvm::Value *Instance) const;

    /// Loads a pointer to the first element of the parameter vector associated
    /// with the given domain_instance object.
    llvm::Value *loadParamVec(llvm::IRBuilder<> &builder,
                              llvm::Value *instance) const;

    /// Loads the domain_view corresponding to a formal parameter from a
    /// domain_instance object, where \p index identifies which parameter is
    /// sought.
    llvm::Value *loadParam(llvm::IRBuilder<> &builder,
                           llvm::Value *instance,
                           unsigned paramIdx) const;

    /// Loads a pointer to the first view in the supplied domain_instance's view
    /// vector.
    llvm::Value *loadViewVec(llvm::IRBuilder<> &builder,
                             llvm::Value *instance) const;

    /// Loads the domain_view of the supplied domain_instance object
    /// corresponding to the signature with the given index.
    llvm::Value *loadView(llvm::IRBuilder<> &builder,
                          llvm::Value *instance,
                          llvm::Value *sigIndex) const;

    /// Loads the domain_view of the supplied domain_instance object
    /// corresponding to the signature with the given index.
    llvm::Value *loadView(llvm::IRBuilder<> &builder,
                          llvm::Value *instance,
                          unsigned sigIndex) const;

private:
    CommaRT &CRT;
    CodeGen &CG;
    const llvm::TargetData &TD;

    /// The name of this type.
    static const std::string theTypeName;

    /// The structure type describing a domain_instance.
    llvm::PATypeHolder theType;
};

//===----------------------------------------------------------------------===//
// DomainInstance::getFieldType specializations for each field in a
// domain_instance.

template <> inline
DomainInstance::FieldIdTraits<DomainInstance::Info>::FieldType *
DomainInstance::getFieldType<DomainInstance::Info>() const {
    typedef FieldIdTraits<Info>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Info));
}

template <> inline
DomainInstance::FieldIdTraits<DomainInstance::Next>::FieldType *
DomainInstance::getFieldType<DomainInstance::Next>() const {
    typedef FieldIdTraits<Next>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Next));
}

template <> inline
DomainInstance::FieldIdTraits<DomainInstance::Params>::FieldType *
DomainInstance::getFieldType<DomainInstance::Params>() const {
    typedef FieldIdTraits<Params>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Params));
}

template <> inline
DomainInstance::FieldIdTraits<DomainInstance::Views>::FieldType *
DomainInstance::getFieldType<DomainInstance::Views>() const {
    typedef FieldIdTraits<Views>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Views));
}

template <> inline
DomainInstance::FieldIdTraits<DomainInstance::Requirements>::FieldType *
DomainInstance::getFieldType<DomainInstance::Requirements>() const {
    typedef FieldIdTraits<Requirements>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Requirements));
}

} // end comma namespace.

#endif
