//===-- comma/CodeGen.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGEN_HDR_GUARD
#define COMMA_CODEGEN_CODEGEN_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/DerivedTypes.h"
#include "llvm/GlobalValue.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Target/TargetData.h"

namespace comma {

class CodeGenCapsule;
class CodeGenTypes;
class CommaRT;
class DependencySet;
class InstanceInfo;
class SRInfo;

class CodeGen {

public:
    ~CodeGen();

    CodeGen(llvm::Module *M, const llvm::TargetData &data,
            AstResource &resource);

    /// \brief Codegens a top-level declaration.
    void emitToplevelDecl(Decl *decl);

    /// \brief Codegens an entry function which calls into the Procedure \p proc
    /// and embeds it into the LibraryItem \p item.
    ///
    /// The given procedure must meet the following constraints (failure to do
    /// so will fire an assertion):
    ///
    ///   - The procedure must be nullary.  Parameters are not accepted.
    ///
    ///   - The procedure must be defined within a domain, not a functor.
    ///
    ///   - The procedure must have been codegened.
    ///
    void emitEntry(ProcedureDecl *pdecl);

    /// Returns the interface to the runtime system.
    const CommaRT &getRuntime() const { return *CRT; }

    /// Returns the module we are generating code for.
    llvm::Module *getModule() { return M; }

    /// Returns the llvm::TargetData used to generate code.
    const llvm::TargetData &getTargetData() const { return TD; }

    /// Returns the AstResource used to generate new AST nodes.
    AstResource &getAstResource() const { return Resource; }

    /// Returns the LLVMContext associated with this code generator.
    llvm::LLVMContext &getLLVMContext() const {
        return llvm::getGlobalContext();
    }

    /// \brief Inserts the given instance into the work list.
    ///
    /// \return true if the instance was not already present in the worklist and
    /// false otherwise.
    ///
    /// The given instance must not be dependent (meaning that
    /// DomainInstanceDecl::isDependent must return false).
    ///
    /// When an instance is inserted into the worklist, a few actions take
    /// place.  First, the instance is schedualed for codegen, meaning that
    /// specializations of that instances subroutines will be emmited into the
    /// current module.  Second, forward declarations are created for each of
    /// the instances subroutines.  These declarations are accessible thru the
    /// lookupGlobal method using the appropriately mangled name.
    bool extendWorklist(DomainInstanceDecl *instace);

    InstanceInfo *lookupInstanceInfo(DomainInstanceDecl *instance) const {
        return instanceTable.lookup(instance);
    }

    InstanceInfo *getInstanceInfo(DomainInstanceDecl *instance) const {
        InstanceInfo *info = lookupInstanceInfo(instance);
        assert(info && "Instance lookup failed!");
        return info;
    }

    /// \brief Returns the SRInfo object associated with \p srDecl.
    ///
    /// The given instance must be a domain registered with the code generator.
    /// If the lookup of \p srDecl fails an assertion will fire.
    SRInfo *getSRInfo(DomainInstanceDecl *instance, SubroutineDecl *srDecl);


    /// FIXME: This method needs to be encapsulated in the seperate structure.
    const DependencySet &getDependencySet(const Domoid *domoid);

    /// \brief Adds a mapping between the given link name and an LLVM
    /// GlobalValue into the global table.
    ///
    /// Returns true if the insertion succeeded and false if there was a
    /// conflict.  In the latter case, the global table is not modified.
    bool insertGlobal(const std::string &linkName, llvm::GlobalValue *GV);

    /// \brief Returns an llvm value for a previously declared decl with the
    /// given link (mangled) name, or null if no such declaration exists.
    llvm::GlobalValue *lookupGlobal(const std::string &linkName) const;

    /// \brief Returns an llvm value representing the static compile time info
    /// representing a previously declared capsule, or null if no such
    /// information is present.
    llvm::GlobalValue *lookupCapsuleInfo(const Domoid *domoid) const;

    /// \brief Emits a string with internal linkage, returning the global
    /// variable for the associated data.  If addNull is true, emit as a null
    /// terminated string.
    llvm::GlobalVariable *emitInternString(const llvm::StringRef &elems,
                                           bool addNull = true,
                                           bool isConstant = true,
                                           const std::string &name = "");

    /// \brief Emits an array with internal linkage, returning the global
    /// variable for the associated data.
    llvm::GlobalVariable *emitInternArray(llvm::Constant *init,
                                          bool isConstant = true,
                                          const std::string &name = "");

    /// Returns an llvm basic block.
    llvm::BasicBlock *makeBasicBlock(const std::string &name = "",
                                     llvm::Function *parent = 0,
                                     llvm::BasicBlock *insertBefore = 0) const;

    /// \brief Returns a global variable with external linkage embedded in the
    /// current module.
    ///
    /// \param init A constant initializer for the global.
    ///
    /// \param isConstant If true, the global will be allocated in a read-only
    /// section, otherwise in a writeable section.
    ///
    /// \param name The name of the global to be linked into the module.
    llvm::GlobalVariable *makeExternGlobal(llvm::Constant *init,
                                           bool isConstant = false,
                                           const std::string &name = "");

    /// \brief Returns a global variable with internal linkage embedded in the
    /// current module.
    ///
    /// \param init A constant initializer for the global.
    ///
    /// \param isConstant If true, the global will be allocated in a read-only
    /// section, otherwise in a writeable section.
    ///
    /// \param name The name of the global to be linked into the module.
    llvm::GlobalVariable *makeInternGlobal(llvm::Constant *init,
                                           bool isConstant = false,
                                           const std::string &name = "");

    /// \brief Creates a function with the given name and type.  The linkage
    /// type is external.
    llvm::Function *makeFunction(const llvm::FunctionType *Ty,
                                 const std::string &name = "");

    /// \brief Creates a function corresponding to the given Comma subroutine
    /// declaration.
    llvm::Function *makeFunction(const DomainInstanceDecl *instance,
                                 const SubroutineDecl *srDecl,
                                 CodeGenTypes &CGT);

    /// \brief Creates a function with the given name and type.  The linkage
    /// type is internal.
    llvm::Function *makeInternFunction(const llvm::FunctionType *Ty,
                                       const std::string &name = "");

    /// Returns a function declaration for the given llvm intrinsic.
    ///
    /// This method is not appropriate for the retrieval of overloaded
    /// intrinsics.
    llvm::Function *getLLVMIntrinsic(llvm::Intrinsic::ID id) {
        assert(!llvm::Intrinsic::isOverloaded(id) &&
               "Cannot retrieve overloaded intrinsics!");
        return llvm::Intrinsic::getDeclaration(M, id);
    }

    /// Returns a function declaration for the llvm.memcpy.i64 intrinsic.
    llvm::Function *getMemcpy64() const;

    /// Returns a pointer to a global exception object.
    llvm::GlobalVariable *getEHInfo();

    /// Returns an llvm "opaque" type.
    const llvm::OpaqueType *getOpaqueTy() const {
        return llvm::OpaqueType::get(getLLVMContext());
    }

    /// Returns the llvm type i8*.
    const llvm::PointerType *getInt8PtrTy() const {
        return getPointerType(getInt8Ty());
    }

    /// \brief Returns the llvm type "void".
    const llvm::Type *getVoidTy() const {
        return llvm::Type::getVoidTy(getLLVMContext());
    }

    /// \brief Returns the llvm type "i1".
    const llvm::IntegerType *getInt1Ty() const {
        return llvm::Type::getInt1Ty(getLLVMContext());
    }

    /// \brief Returns the llvm type "i8".
    const llvm::IntegerType *getInt8Ty() const {
        return llvm::Type::getInt8Ty(getLLVMContext());
    }

    /// \brief Returns the llvm type "i16".
    const llvm::IntegerType *getInt16Ty() const {
        return llvm::Type::getInt16Ty(getLLVMContext());
    }

    /// \brief Returns the llvm type "i32".
    const llvm::IntegerType *getInt32Ty() const {
        return llvm::Type::getInt32Ty(getLLVMContext());
    }

    /// \brief Returns the llvm type "i64".
    const llvm::IntegerType *getInt64Ty() const {
        return llvm::Type::getInt64Ty(getLLVMContext());
    }

    /// Returns a null pointer constant of the specified type.
    llvm::Constant *getNullPointer(const llvm::PointerType *Ty) const {
        return llvm::ConstantPointerNull::get(Ty);
    }

    /// Returns a pointer-to the given type.
    llvm::PointerType *getPointerType(const llvm::Type *Ty) const {
        return llvm::PointerType::getUnqual(Ty);
    }

    /// Returns a constant integer.
    llvm::ConstantInt *getConstantInt(const llvm::IntegerType *type,
                                      uint64_t value) const {
        return llvm::ConstantInt::get(type, value);
    }

    /// Returns a ConstantInt for the given value.
    ///
    /// This method ensures that the given APInt is within the representational
    /// limits of the given type.  If the bit width of the supplied APInt does
    /// not match that of the given type, then the active bits of the value
    /// (interpreted as a signed integer) are used, sign extended to the width
    /// of the type.  An assertion will fire if the number of active bits
    /// exceeds the width of the supplied type.
    llvm::ConstantInt *getConstantInt(const llvm::IntegerType *type,
                                      const llvm::APInt &value) const;

    /// \brief Returns a constant array consiting of the given elements, each of
    /// which must be of the supplied type.
    llvm::Constant *
    getConstantArray(const llvm::Type *elementType,
                     std::vector<llvm::Constant*> &elems) const {
        llvm::ArrayType *arrayTy;
        arrayTy = llvm::ArrayType::get(elementType, elems.size());
        return llvm::ConstantArray::get(arrayTy, elems);

    }

    /// Casts the given constant expression into the given pointer type.
    llvm::Constant *getPointerCast(llvm::Constant *constant,
                                   const llvm::PointerType *Ty) const {
        return llvm::ConstantExpr::getPointerCast(constant, Ty);
    }

    /// Returns an llvm structure type.
    llvm::StructType *getStructTy(const std::vector<const llvm::Type*> &elts,
                                  bool isPacked = false) const {
        return llvm::StructType::get(getLLVMContext(), elts, isPacked);
    }

private:
    /// The Module we are emiting code for.
    llvm::Module *M;

    /// Data describing our target,
    const llvm::TargetData &TD;

    /// The AstResource used for generating new types.
    AstResource &Resource;

    /// Interface to the runtime system.
    CommaRT *CRT;

    /// The type of table used to map strings to global values.
    typedef llvm::StringMap<llvm::GlobalValue *> StringGlobalMap;

    /// A map from the link name of a capsule to its corresponding info table.
    StringGlobalMap capsuleInfoTable;

    /// A map from declaration names to LLVM global values.
    StringGlobalMap globalTable;

    /// Table mapping domain instance declarations to the corresponding
    /// InstanceInfo objects.
    typedef llvm::DenseMap<DomainInstanceDecl*, InstanceInfo*> InstanceMap;
    InstanceMap instanceTable;

    /// FIXME: Temporary mapping from Domoids to their dependency sets.  This
    /// information will be encapsulated in an as-yet undefined class.
    typedef llvm::DenseMap<const Domoid*, DependencySet*> DependenceMap;
    DependenceMap dependenceTable;

    /// Generates an InstanceInfo object and adds it to the instance table.
    ///
    /// This method will assert if there already exists an info object for the
    /// given instance.
    InstanceInfo *createInstanceInfo(DomainInstanceDecl *instance);

    /// Returns true if there exists an member in the instance table which needs
    /// to be compiled.
    bool instancesPending() const;

    /// Compiles the next member of the instance table.  This operation could
    /// very well expand the table to include more instances.
    void emitNextInstance();
};

} // end comma namespace.

#endif
