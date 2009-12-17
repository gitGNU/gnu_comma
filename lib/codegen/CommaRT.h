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

#include "llvm/ADT/DenseMap.h"
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

    llvm::GlobalVariable *registerCapsule(Domoid *domoid);

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

    /// \name Exception Handling.
    ///
    /// The following methods provide access to the exception handling component
    /// of Comma's runtime.
    //@{

    /// \brief Registers and exception with the runtime.
    ///
    /// Given an ExceptionDecl AST node, this method returns an opaque global
    /// representing the exception.  The first call made to this method with a
    /// given ExceptionDecl as argument registers the exception in the system
    /// and associates a global as representation.  Subsequent calls using the
    /// same declaration node return the same global.
    llvm::Constant *registerException(const ExceptionDecl *decl);

    /// Throws an exception.
    ///
    /// Calls registerException on the provided exception declaration, then
    /// generates code for a raise.  \p message must be a pointer to a global
    /// string containing or null.
    void raise(llvm::IRBuilder<> &builder, const ExceptionDecl *decl,
               llvm::GlobalVariable *message);

    /// Convinience method to throw a PROGRAM_ERROR.
    void raiseProgramError(llvm::IRBuilder<> &builder,
                           llvm::GlobalVariable *message) const;

    /// Convinience method to throw a CONSTRAINT_ERROR.
    void raiseConstraintError(llvm::IRBuilder<> &builder,
                              llvm::GlobalVariable *message) const;

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
    //@}

    /// Integer exponentiation routines.
    //@{
    llvm::Value *pow_i32_i32(llvm::IRBuilder<> &builder,
                             llvm::Value *x, llvm::Value *n) const;
    llvm::Value *pow_i64_i32(llvm::IRBuilder<> &builder,
                             llvm::Value *x, llvm::Value *n) const;
    //@}


    /// Variable stack routines.
    //@{
    /// Allocates \p size bytes of uninitialized data onto the vstack
    /// (accessable thru vstack()).
    void vstack_alloc(llvm::IRBuilder<> &builder, llvm::Value *size) const;

    /// Pushes \p size bytes from \p data onto the variable stack.
    void vstack_push(llvm::IRBuilder<> &builder,
                     llvm::Value *data, llvm::Value *size) const;

    /// Pops the last item pushed from the variable stack.
    void vstack_pop(llvm::IRBuilder<> &builder) const;

    /// Returns a pointer to the most recent data pushed onto the variable
    /// stack cast to the given type.
    llvm::Value *vstack(llvm::IRBuilder<> &builder,
                        const llvm::Type *ptrTy) const;
    //@}

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
    std::string pow_i32_i32_Name;
    std::string pow_i64_i32_Name;

    // Function declarations for the comma runtime functions.
    llvm::Function *getDomainFn;
    llvm::Function *assertFailFn;
    llvm::Function *EHPersonalityFn;
    llvm::Function *unhandledExceptionFn;
    llvm::Function *raiseExceptionFn;
    llvm::Function *pow_i32_i32_Fn;
    llvm::Function *pow_i64_i32_Fn;
    llvm::Function *vstack_alloc_Fn;
    llvm::Function *vstack_push_Fn;
    llvm::Function *vstack_pop_Fn;

    // Runtime global variables.
    llvm::GlobalVariable *vstack_Var;

    // Mapping from user-defined exceptions to the llvm::GlobalVariable's that
    // contain their associated comma_exinfo objects.
    typedef llvm::DenseMap<const ExceptionDecl*,
                           llvm::GlobalVariable*> ExceptionMap;
    ExceptionMap registeredExceptions;

    // External globals representing the language defined exceptions.
    // Definitions of these globals are provided by libruntime.
    llvm::GlobalVariable *theProgramErrorExinfo;
    llvm::GlobalVariable *theConstraintErrorExinfo;

    const llvm::PointerType *getDomainCtorPtrTy();
    const llvm::PointerType *getITablePtrTy();

    // Methods which build the LLVM IR for the comma runtime functions.
    void defineGetDomain();
    void defineAssertFail();
    void defineEHPersonality();
    void defineUnhandledException();
    void defineRaiseException();
    void defineExinfos();
    void define_pow_i32_i32();
    void define_pow_i64_i32();
    void define_vstack_alloc();
    void define_vstack_push();
    void define_vstack_pop();
    void define_vstack();

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

    /// Generates a constant comma_exinfo_t initializer for the given exception
    /// declaration.
    llvm::Constant *genExinfoInitializer(const ExceptionDecl *exception);
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

