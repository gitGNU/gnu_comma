//===-- codegen/CommaRT.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_COMMART_HDR_GUARD
#define COMMA_CODEGEN_COMMART_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/Support/IRBuilder.h"

namespace llvm {

class TargetData;

} // end llvm namespace;

namespace comma {

class CodeGen;
class CodeGenCapsule;
class DomainInfo;
class DomainInstance;
class DomainView;
class ExportMap;
class SignatureSet;

class CommaRT {

public:
    CommaRT(CodeGen &CG);

    ~CommaRT();

    /// \brief Returns the CodeGen object over which this runtime was
    /// constructed.
    CodeGen &getCodeGen() { return CG; }
    const CodeGen &getCodeGen() const { return CG; }

    /// \brief Returns the ExportMap which maintains the inter-module export
    /// tables.
    ExportMap &getExportMap() { return *EM; }
    const ExportMap &getExportMap() const { return *EM; }

    enum TypeId {
        CRT_ExportFn,
        CRT_ITable,
        CRT_DomainInfo,
        CRT_DomainView,
        CRT_DomainInstance,
        CRT_DomainCtor
    };

    template <TypeId F>
    struct TypeIdTraits {
        typedef const llvm::PointerType FieldType;
    };

    template <TypeId F>
    typename TypeIdTraits<F>::FieldType *getType() const;

    const std::string &getTypeName(TypeId id) const;

    llvm::GlobalVariable *registerCapsule(CodeGenCapsule &CGC);

    void registerSignature(const Sigoid *sigoid);

    llvm::Value *getDomain(llvm::IRBuilder<> &builder,
                           llvm::GlobalValue *capsuleInfo) const;

    llvm::Value *getDomain(llvm::IRBuilder<> &builder,
                           std::vector<llvm::Value*> &args) const;

    llvm::Value *getLocalCapsule(llvm::IRBuilder<> &builder,
                                 llvm::Value *percent, unsigned ID) const;

    llvm::Value *genAbstractCall(llvm::IRBuilder<> &builder,
                                 llvm::Value *percent,
                                 const SubroutineDecl *srDecl,
                                 const std::vector<llvm::Value *> &args) const;


    unsigned getSignatureOffset(DomainValueDecl *dom, SignatureType *target);
    unsigned getSignatureOffset(Domoid *dom, SignatureType *target);

    /// The following methods are not for public consumption.  They provide
    /// access to objects used in other areas of the runtime coegen system.
    const DomainInfo *getDomainInfo() const { return DInfo; }
    const DomainView *getDomainView() const { return DView; }
    const DomainInstance *getDomainInstance() const { return DInstance; }

private:

    CodeGen &CG;

    /// The export map maintaining the signature lookup tables.
    ExportMap *EM;

    // Names of the basic runtime types as they appear in llvm IR.
    std::string InvalidName;
    std::string ITableName;
    std::string DomainCtorName;

    DomainInfo *DInfo;
    const llvm::PointerType *DomainInfoPtrTy;

    DomainView *DView;
    const llvm::PointerType *DomainViewPtrTy;

    DomainInstance *DInstance;
    const llvm::PointerType *DomainInstancePtrTy;

    const llvm::PointerType *ExportFnPtrTy;
    const llvm::PointerType *ITablePtrTy;
    const llvm::PointerType *DomainCtorPtrTy;

    std::string GetDomainName;

    // Function declaration for _comma_get_domain runtime function.
    llvm::Function *getDomainFn;

    const llvm::PointerType *getDomainCtorPtrTy();
    const llvm::PointerType *getExportFnPtrTy();
    const llvm::PointerType *getITablePtrTy();

    // Builds a declaration in LLVM IR for the get_domain runtime function.
    void defineGetDomain();

    // Builds the llvm IR for the primitive types needed by the runtime system.
    void generateRuntimeTypes();

    // Builds the llvm IR declarations for the primitive functions provided by
    // the runtime library.
    void generateRuntimeFunctions();

    // Builds the constructor function for the given capsule.
    llvm::Constant *genCapsuleCtor(CodeGenCapsule &CGC);

    // Helper method for genCapsuleCtor.  Generates a call to get_domain for the
    // given instance decl, storing the result into the destination vector with
    // the given index.
    void genInstanceRequirement(llvm::IRBuilder<> &builder,
                                CodeGenCapsule &CGC,
                                unsigned ID,
                                llvm::Value *destVector,
                                llvm::Value *percent);

    /// Helper method for genInstanceRequirement.  Generates a call to
    /// get_domain for the given instance (which must be an instance of a
    /// non-parameterized domain), storing the result into the destination
    /// vector with the given index.
    void genDomainRequirement(llvm::IRBuilder<> &builder,
                              CodeGenCapsule &CGC,
                              unsigned ID,
                              llvm::Value *destVector);

    /// Helper method for genInstanceRequirement.  Generates a call to
    /// get_domain for the given instance (which must be an instance of a
    /// functor), storing the result into the destination vector with the given
    /// index.
    void genFunctorRequirement(llvm::IRBuilder<> &builder,
                               CodeGenCapsule &CGC,
                               unsigned ID,
                               llvm::Value *destVector,
                               llvm::Value *percent);

    // Generates an llvm IR integer constant representing the number of
    // supersignatures implemented by the given capsule.
    llvm::ConstantInt *genSignatureCount(CodeGenCapsule &CGC);

    llvm::Constant *genSignatureOffsets(CodeGenCapsule &CGC);

    llvm::Constant *genExportArray(CodeGenCapsule &CGC);

    unsigned
    emitOffsetsForSignature(SignatureType *sig,
                            unsigned index,
                            const llvm::Type *elemTy,
                            std::vector<llvm::Constant *> &offsets);

    unsigned getSignatureOffset(const SignatureSet &sigset,
                                SignatureType *target);

};

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_ExportFn>::FieldType *
CommaRT::getType<CommaRT::CRT_ExportFn>() const {
    return ExportFnPtrTy;
}

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_ITable>::FieldType *
CommaRT::getType<CommaRT::CRT_ITable>() const {
    return ExportFnPtrTy;
}

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_DomainInfo>::FieldType *
CommaRT::getType<CommaRT::CRT_DomainInfo>() const {
    return DomainInfoPtrTy;
}

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_DomainView>::FieldType *
CommaRT::getType<CommaRT::CRT_DomainView>() const {
    return DomainViewPtrTy;
}

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_DomainInstance>::FieldType *
CommaRT::getType<CommaRT::CRT_DomainInstance>() const {
    return DomainInstancePtrTy;
}

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_DomainCtor>::FieldType *
CommaRT::getType<CommaRT::CRT_DomainCtor>() const {
    return DomainCtorPtrTy;
}

} // end comma namespace.

#endif

