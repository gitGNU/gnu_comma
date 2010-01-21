//===-- codegen/CommaRT.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
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
class DomainInfo;
class DomainInstance;
class SRFrame;

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
    llvm::Constant *registerException(const ExceptionDecl *exception);

    /// Throws an exception.
    ///
    /// Calls registerException on the provided exception declaration, then
    /// generates code for a raise.  \p fileName is an i8* yeilding the name of
    /// the file or module the exception is raised in and \p lineNum is the
    /// corresponding line number.  \p message must be a pointer to a global
    /// string of type i8* or null.
    void raise(SRFrame *frame, const ExceptionDecl *exception,
               llvm::Value *fileName, llvm::Value *lineNum,
               llvm::GlobalVariable *message = 0);

    /// Throws an exception.
    ///
    /// Calls registerException on the provided exception declaration, then
    /// generates code for a raise.  \p fileName is an i8* yeilding the name of
    /// the file or module the exception is raised in and \p lineNum is the
    /// corresponding line number. \p message is a Comma vector (one dimension)
    /// of type String and \p length is its length (an i32).  The supplied
    /// vector may be null.
    ///
    /// \note For compiler generated exceptions it is always preferable to raise
    /// using a static global as the runtime can avoid a copy of the message
    /// data in that case.
    void raise(SRFrame *frame, const ExceptionDecl *exception,
               llvm::Value *fileName, llvm::Value *lineNum,
               llvm::Value *message = 0, llvm::Value *length = 0);

    /// Reraises the given exception object.
    void reraise(SRFrame *frame, llvm::Value *exception);

    /// Convinience method to throw a Program_Error.
    void raiseProgramError(SRFrame *frame,
                           llvm::Value *fileName, llvm::Value *lineNum,
                           llvm::GlobalVariable *message) const;

    /// Convinience method to throw a Constraint_Error.
    void raiseConstraintError(SRFrame *frame,
                              llvm::Value *fileName, llvm::Value *lineNum,
                              llvm::GlobalVariable *message) const;

    /// Convinience method to throw a Assertion_Error.
    void raiseAssertionError(SRFrame *frame,
                             llvm::Value *fileName, llvm::Value *lineNum,
                             llvm::Value *message, llvm::Value *length) const;

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


    /// \name Variable stack routines.
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

    /// \name Memory allocation routines.
    //@{
    llvm::Value *comma_alloc(llvm::IRBuilder<> &builder,
                             uint64_t size, unsigned alignment) const;

    llvm::Value *comma_alloc(llvm::IRBuilder<> &builder,
                             llvm::Value *size, unsigned alignment) const;

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

    // Function declarations for the comma runtime functions.
    llvm::Function *getDomainFn;
    llvm::Function *EHPersonalityFn;
    llvm::Function *unhandledExceptionFn;
    llvm::Function *raiseStaticExceptionFn;
    llvm::Function *raiseUserExceptionFn;
    llvm::Function *reraiseExceptionFn;
    llvm::Function *pow_i32_i32_Fn;
    llvm::Function *pow_i64_i32_Fn;
    llvm::Function *vstack_alloc_Fn;
    llvm::Function *vstack_push_Fn;
    llvm::Function *vstack_pop_Fn;
    llvm::Function *alloc_Fn;

    // Runtime global variables.
    llvm::GlobalVariable *vstack_Var;

    // Mapping from user-defined exceptions to the llvm::GlobalVariable's that
    // contain their associated comma_exinfo objects.
    typedef llvm::DenseMap<const ExceptionDecl*,
                           llvm::GlobalVariable*> ExceptionMap;
    ExceptionMap registeredExceptions;

    // External globals representing the language defined exceptions.
    // Definitions of these globals are provided by libruntime.
    llvm::Constant *theProgramErrorExinfo;
    llvm::Constant *theConstraintErrorExinfo;
    llvm::Constant *theAssertErrorExinfo;

    const llvm::PointerType *getDomainCtorPtrTy();
    const llvm::PointerType *getITablePtrTy();

    // Methods which build the LLVM IR for the comma runtime functions.
    void defineGetDomain();
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
    void define_alloc();

    // Builds the llvm IR for the primitive types needed by the runtime system.
    void generateRuntimeTypes();

    // Builds the llvm IR declarations for the primitive functions provided by
    // the runtime library.
    void generateRuntimeFunctions();

    /// Generates a constant comma_exinfo_t initializer for the given exception
    /// declaration.
    llvm::Constant *genExinfoInitializer(const ExceptionDecl *exception);

    /// Helper method to the raise methods.  Ensures the given global value
    /// denotes a CString if non-null.  If null, returns a null i8*.
    llvm::Constant *checkAndConvertMessage(llvm::GlobalVariable *message) const;

    /// Helper method for the various exception generators.  Raises an exception
    /// given an exinfo object and a static message.
    void raiseExinfo(SRFrame *frame, llvm::Value *exinfo,
                     llvm::Value *fileName, llvm::Value *lineNum,
                     llvm::GlobalVariable *message) const;

    /// Helper method for the various exception generators.  Raises an exception
    /// given an exinfo object and a dynammic message.
    void raiseExinfo(SRFrame *frame, llvm::Value *exinfo,
                     llvm::Value *fileName, llvm::Value *lineNum,
                     llvm::Value *message, llvm::Value *length) const;
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

