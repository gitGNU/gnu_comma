//===-- codegen/DomainView.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_DOMAINVIEW_HDR_GUARD
#define COMMA_CODEGEN_DOMAINVIEW_HDR_GUARD

#include "comma/codegen/CodeGen.h"
#include "llvm/Support/IRBuilder.h"

namespace comma {

class CommaRT;

class DomainView {

public:
    DomainView(CommaRT &CRT);

    void init();

    enum FieldId {
        Instance,
        Index
    };

    /// Returns the name of a domain_view type.
    const std::string &getTypeName() const { return theTypeName; }

    /// Returns a structure type describing a domain_view object.
    const llvm::StructType *getType() const;

    /// Returns a pointer-to domain_view structure type.
    const llvm::PointerType *getPointerTypeTo() const;

    /// Loads the domain_instance associated with the given domain_view.
    llvm::Value *loadInstance(llvm::IRBuilder<> &builder,
                              llvm::Value *DView) const;

    /// Loads the signature index associated with the given domain_view.
    llvm::Value *loadIndex(llvm::IRBuilder<> &builder, llvm::Value *View) const;

    /// Downcasts the provided domain_view to another view corresponding to the
    /// signature with the given (view-relative) index.
    llvm::Value *downcast(llvm::IRBuilder<> &builder,
                          llvm::Value *view,
                          unsigned sigIndex) const;

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

    /// The structure type describing a domain_view.
    llvm::PATypeHolder theType;
};


//===----------------------------------------------------------------------===//
// DomainView::FieldIdTraits specializations for the various fields within a
// domain_view.

template <>
struct DomainView::FieldIdTraits<DomainView::Index> {
    typedef const llvm::IntegerType FieldType;
};

//===----------------------------------------------------------------------===//
// DomainView::getFieldType specializations for each field in a domain_view.

template <> inline
DomainView::FieldIdTraits<DomainView::Instance>::FieldType *
DomainView::getFieldType<DomainView::Instance>() const {
    typedef DomainView::FieldIdTraits<DomainView::Instance>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Instance));
}

template <> inline
DomainView::FieldIdTraits<DomainView::Index>::FieldType *
DomainView::getFieldType<DomainView::Index>() const {
    typedef DomainView::FieldIdTraits<DomainView::Index>::FieldType FTy;
    return llvm::cast<FTy>(getType()->getElementType(Index));
}

} // end comma namespace.

#endif
