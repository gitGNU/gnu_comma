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

namespace comma {

class CodeGen;
class CodeGenCapsule;

class CommaRT {

public:
    CommaRT(CodeGen &CG);

    enum TypeId {
        CRT_ExportFn,
        CRT_ITable,
        CRT_DomainInfo,
        CRT_DomainView,
        CRT_DomainInstance,
        CRT_DomainCtor
    };

    const llvm::Type *getType(TypeId id) const;
    const std::string &getTypeName(TypeId id) const;

    llvm::GlobalVariable *registerCapsule(CodeGenCapsule &CGC);

    llvm::Value *getDomain(llvm::IRBuilder<> &builder,
                           llvm::GlobalValue *capsuleInfo) const;

    llvm::Value *getLocalCapsule(llvm::IRBuilder<> &builder,
                                 llvm::Value *percent, unsigned ID) const;

private:
    class DomainInfoTy {

    public:
        DomainInfoTy(const llvm::TargetData &TD,
                     const llvm::PointerType *CtorTy,
                     const llvm::PointerType *ITableTy,
                     const llvm::PointerType *ExportFnTy);

        enum FieldID {
            Name,
            Arity,
            Ctor,
            ITable,
            NumSigs,
            SigOffsets,
            Exvec
        };

        const std::string &getTypeName() const { return theTypeName; }
        const llvm::StructType *getType() const { return theType; }
        const llvm::PointerType *getPointerTypeTo() const;

        const llvm::Type *getFieldType(FieldID ID) const;

    private:
        const llvm::TargetData &TD;
        llvm::StructType *theType;

        static const std::string theTypeName;
    };

    CodeGen &CG;

    // Names of the basic runtime types as they appear in llvm IR.
    std::string InvalidName;
    std::string ITableName;
    std::string ExportFnName;
    std::string DomainViewName;
    std::string DomainInstanceName;
    std::string DomainCtorName;

    llvm::PointerType *ExportFnPtrTy;
    llvm::PointerType *ITablePtrTy;

    llvm::PATypeHolder  DomainViewTy;
    llvm::PATypeHolder  DomainInstanceTy;

    llvm::PointerType *DomainViewPtrTy;
    llvm::PointerType *DomainInstancePtrTy;
    llvm::PointerType *DomainCtorPtrTy;

    DomainInfoTy DIType;

    std::string GetDomainName;

    // Function declaration for _comma_get_domain runtime function.
    llvm::Function *getDomainFn;


    llvm::PointerType *getDomainCtorPtrTy();
    llvm::PointerType *getExportFnPtrTy();
    llvm::PointerType *getITablePtrTy();

    void defineITableTy();
    void defineDomainInfoTy();
    void defineDomainViewTy();
    void defineDomainInstanceTy();

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
    unsigned genInstanceRequirement(llvm::IRBuilder<> &builder,
                                    DomainInstanceDecl *instanceDecl,
                                    unsigned index,
                                    llvm::Value *destVector);

    // Generates an llvm IR integer constant representing the number of
    // supersignatures implemented by the given capsule.
    llvm::ConstantInt *genSignatureCount(CodeGenCapsule &CGC);

    llvm::Constant *genSignatureOffsets(CodeGenCapsule &CGC);

    unsigned
    emitOffsetsForSignature(SignatureType *sig,
                            unsigned index,
                            const llvm::Type *elemTy,
                            std::vector<llvm::Constant *> &offsets);

};

} // end comma namespace.

#endif

