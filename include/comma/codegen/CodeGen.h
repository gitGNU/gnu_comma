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
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Target/TargetData.h"

#include <string>
#include <map>

namespace comma {

class CodeGenCapsule;
class CodeGenTypes;
class CommaRT;

class CodeGen {

public:
    ~CodeGen();

    CodeGen(llvm::Module *M, const llvm::TargetData &data,
            AstResource &resource);

    /// \brief Returns the type generator used to lower Comma AST types into
    /// LLVM IR types.
    const CodeGenTypes &getTypeGenerator() const;
    CodeGenTypes &getTypeGenerator();

    /// \brief Returns the current capsule generator.
    const CodeGenCapsule &getCapsuleGenerator() const;
    CodeGenCapsule &getCapsuleGenerator();

    /// \brief Returns the interface to the runtime system.
    const CommaRT &getRuntime() const { return *CRT; }

    /// \brief Returns the module we are generating code for.
    llvm::Module *getModule() { return M; }

    /// \brief Returns the llvm::TargetData used to generate code.
    const llvm::TargetData &getTargetData() const { return TD; }

    /// \brief Returns the AstResource used to generate new AST nodes.
    AstResource &getAstResource() const { return Resource; }

    /// \brief Returns the LLVMContext associated with this code generator.
    llvm::LLVMContext &getLLVMContext() const {
        return llvm::getGlobalContext();
    }

    /// \brief Codegens a top-level declaration.
    void emitToplevelDecl(Decl *decl);

    /// \brief Codegens a main function which calls into the given Comma
    /// procedure.
    ///
    /// The provided procedure must meet the following constraints (failure to
    /// do so will fire an assertion):
    ///
    ///   - The procedure must be nullary.  Parameters are not accepted.
    ///
    ///   - The procedure must be defined within a domain, not a functor.
    ///
    ///   - The procedure must have been codegened.
    ///
    void emitEntryStub(ProcedureDecl *pdecl);

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
    /// specialisations of that instances subroutines will be emmited into the
    /// current module.  Second, forward declarations are created for each of
    /// the instances subroutines.  These declarations are accessible thru the
    /// lookupGlobal method using the appropriately mangled name.
    bool extendWorklist(DomainInstanceDecl *instace);

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
    llvm::GlobalValue *lookupCapsuleInfo(Domoid *domoid) const;

    /// \brief Emits a string with internal linkage, returning the global
    /// variable for the associated data.
    llvm::Constant *emitStringLiteral(const std::string &str,
                                      bool isConstant = true,
                                      const std::string &name = "");

    /// \brief Returns a null pointer constant of the specified type.
    llvm::Constant *getNullPointer(const llvm::PointerType *Ty) const;

    /// \brief Returns an llvm "opaque" type.
    const llvm::OpaqueType *getOpaqueTy() const {
        return llvm::OpaqueType::get(getLLVMContext());
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


    /// \breif Returns an llvm basic block.
    llvm::BasicBlock *makeBasicBlock(const std::string &name = "",
                                     llvm::Function *parent = 0,
                                     llvm::BasicBlock *insertBefore = 0) const;

    /// \breif Returns an llvm structure type.
    llvm::StructType *getStructTy(const std::vector<const llvm::Type*> &params,
                                  bool isPacked = false) {
        return llvm::StructType::get(getLLVMContext(), params, isPacked);
    }

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

    /// \brief Creates a function with the given name and type.  The linkage
    /// type is internal.
    llvm::Function *makeInternFunction(const llvm::FunctionType *Ty,
                                       const std::string &name = "");

    /// \brief Casts the given constant expression into the given pointer type.
    llvm::Constant *getPointerCast(llvm::Constant *constant,
                                   const llvm::PointerType *Ty) const;

    /// \brief Returns a pointer-to the given type.
    llvm::PointerType *getPointerType(const llvm::Type *Ty) const;


    /// \brief Returns a constant array consiting of the given elements, each of
    /// which must be of the supplied type.
    llvm::Constant *getConstantArray(const llvm::Type *elementType,
                                     std::vector<llvm::Constant*> &elems) const;


private:
    /// The Module we are emiting code for.
    llvm::Module *M;

    /// The TargetData describing our target,
    const llvm::TargetData &TD;

    /// The AstResource used for generating new types.
    AstResource &Resource;

    /// The type generator used to lower Comma AST Types into LLVM IR types.
    CodeGenTypes *CGTypes;

    /// Interface to the runtime system.
    CommaRT *CRT;

    /// The current capsule code generator.
    CodeGenCapsule *CGCapsule;

    /// The type of table used to map strings to global values.
    typedef llvm::StringMap<llvm::GlobalValue *> StringGlobalMap;

    /// A map from the link name of a capsule to its corresponding info table.
    StringGlobalMap capsuleInfoTable;

    /// A map from declaration names to LLVM global values.
    StringGlobalMap globalTable;

    /// The work list is represented as a std::map from DomainInstanceDecl's to
    /// WorkEntry structures.  Currently, WorkEntry's contain a single bit of
    /// information : a flag indicating if the associated instance has been
    /// compiled.  This will be extended over time.
    struct WorkEntry {
        DomainInstanceDecl *instance;
        bool isCompiled;
    };

    typedef std::map<DomainInstanceDecl*, WorkEntry> WorkingSet;
    WorkingSet workList;

    /// Returns true if there exists an instance in the worklist which needs to
    /// be compiled.
    bool instancesPending();

    /// Compiles the next instance in the worklist.  This operation could very
    /// well expand the worklist to include more instances.
    void emitNextInstance();
};

}; // end comma namespace

#endif
