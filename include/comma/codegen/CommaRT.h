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

class CommaRT {

public:
    CommaRT(CodeGen &CG);

    ~CommaRT();

    /// \brief Returns the CodeGen object over which this runtime was
    /// constructed.
    CodeGen &getCodeGen() { return CG; }
    const CodeGen &getCodeGen() const { return CG; }

    enum TypeId {
        CRT_ITable,
        CRT_DomainInfo,
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

    llvm::Value *getDomain(llvm::IRBuilder<> &builder,
                           llvm::GlobalValue *capsuleInfo) const;

    llvm::Value *getDomain(llvm::IRBuilder<> &builder,
                           std::vector<llvm::Value*> &args) const;

    llvm::Value *getLocalCapsule(llvm::IRBuilder<> &builder,
                                 llvm::Value *percent, unsigned ID) const;

    /// Returns the formal parameter from the given domain instance with the
    /// given index.
    llvm::Value *getCapsuleParameter(llvm::IRBuilder<> &builder,
                                     llvm::Value *instance,
                                     unsigned index) const;

    /// The following methods are not for public consumption.  They provide
    /// access to objects used in other areas of the runtime codegen system.
    const DomainInfo *getDomainInfo() const { return DInfo; }
    const DomainInstance *getDomainInstance() const { return DInstance; }

    /// Generates a call to _comma_assert_fail using the given message (which
    /// must be a i8*, pointing to a null-terminated C string).
    ///
    /// A call to _comma_assert_fail does not return.
    void assertFail(llvm::IRBuilder<> &builder, llvm::Value *message) const;

    /// Throws an exception.
    ///
    /// Currently, Comma supports only a single "system exception".  The
    /// following call generates the code for a raise, using the given global as
    /// a message.
    void raise(llvm::IRBuilder<> &builder, llvm::GlobalVariable *message) const;

    /// Generates a call to _comma_unhandled_exception.  This is only called by
    /// the main routine when an exception has unwound the entire stack.  Its
    /// only argument is the unhandled exception object.
    ///
    /// A call to _comma_unhandled_exception does not return.
    void unhandledException(llvm::IRBuilder<> &builder,
                            llvm::Value *exception) const;

    /// Returns an opaque reference to the exception handling personality
    /// routine.  Suitable for use as an argument to llvm.eh.selector.
    llvm::Constant *getEHPersonality() const;

private:
    CodeGen &CG;

    // Names of the basic runtime types as they appear in llvm IR.
    std::string InvalidName;
    std::string ITableName;
    std::string DomainCtorName;

    DomainInfo *DInfo;
    const llvm::PointerType *DomainInfoPtrTy;

    DomainInstance *DInstance;
    const llvm::PointerType *DomainInstancePtrTy;

    const llvm::PointerType *ITablePtrTy;
    const llvm::PointerType *DomainCtorPtrTy;

    // Names of the comma runtime functions.
    std::string GetDomainName;
    std::string AssertFailName;
    std::string EHPersonalityName;
    std::string UnhandledExceptionName;
    std::string RaiseExceptionName;

    // Function declarations for the comma runtime functions.
    llvm::Function *getDomainFn;
    llvm::Function *assertFailFn;
    llvm::Function *EHPersonalityFn;
    llvm::Function *unhandledExceptionFn;
    llvm::Function *raiseExceptionFn;

    const llvm::PointerType *getDomainCtorPtrTy();
    const llvm::PointerType *getITablePtrTy();

    // Methods which build the LLVM IR for the comma runtime functions.
    void defineGetDomain();
    void defineAssertFail();
    void defineEHPersonality();
    void defineUnhandledException();
    void defineRaiseException();

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
};

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_ITable>::FieldType *
CommaRT::getType<CommaRT::CRT_ITable>() const {
    return ITablePtrTy;
}

template <> inline
CommaRT::TypeIdTraits<CommaRT::CRT_DomainInfo>::FieldType *
CommaRT::getType<CommaRT::CRT_DomainInfo>() const {
    return DomainInfoPtrTy;
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

