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

#include "llvm/GlobalValue.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Target/TargetData.h"

#include <string>

namespace comma {

class CodeGenCapsule;
class CodeGenTypes;
class CommaRT;

class CodeGen {

public:
    ~CodeGen();

    CodeGen(llvm::Module *M, const llvm::TargetData &data);

    /// \brief Returns the type generator used to lower Comma AST types into
    /// LLVM IR types.
    const CodeGenTypes &getTypeGenerator() const;

    /// \brief Returns the type generator used to lower Comma AST types into
    /// LLVM IR types.
    CodeGenTypes &getTypeGenerator();

    /// \brief Returns the interface to the runtime system.
    const CommaRT &getRuntime() const { return *CRT; }

    /// \brief Returns the module we are generating code for.
    llvm::Module *getModule() { return M; }

    /// \brief Returns the llvm::TargetData used to generate code.
    const llvm::TargetData &getTargetData() const { return TD; }

    /// \brief Codegens a top-level declaration.
    void emitToplevelDecl(Decl *decl);

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

    /// \brief Returns the qualification prefix used to form LLVM IR
    /// identifiers.
    static std::string getLinkPrefix(const Decl *decl);

    /// \brief Returns the name of the given subroutine as it should appear in
    /// LLVM IR.
    ///
    /// The conventions followed by Comma model those of Ada.  In particular, a
    /// subroutines link name is similar to its fully qualified name, except
    /// that the double colon is replaced by an underscore, and overloaded names
    /// are identified using a postfix number.  For example, the Comma name
    /// "D::Foo" is translated into "D__Foo" and subsequent overloads are
    /// translated into "D__Foo__1", "D__Foo__2", etc.  Operator names like "*"
    /// or "+" are given names beginning with a zero followed by a spelled out
    /// alternative.  For example, "*" translates into "0multiply" and "+"
    /// translates into "0plus" (with appropriate qualification prefix and
    /// overload suffix).
    static std::string getLinkName(const SubroutineDecl *sr);

    /// \brief Returns the name of the given Domoid as it should appear in LLVM
    /// IR.
    static std::string getLinkName(const Domoid *domoid);

private:
    /// The Module we are emiting code for.
    llvm::Module *M;

    /// The TargetData describing our target,
    const llvm::TargetData &TD;

    /// The type generator used to lower Comma AST Types into LLVM IR types.
    CodeGenTypes *CGTypes;

    /// Interface to the runtime system.
    CommaRT *CRT;

    /// The type of table used to map strings to global values.
    typedef llvm::StringMap<llvm::GlobalValue *> StringGlobalMap;

    /// A map from the link name of a capsule to its corresponding info table.
    StringGlobalMap capsuleInfoTable;

    /// A map from declaration names to LLVM global values.
    StringGlobalMap globalTable;

    /// \brief Returns the index of a decl within a declarative region.
    ///
    /// This function scans the given region for the given decl.  For each
    /// overloaded name matching that of the decl, the index returned is
    /// incremented (and since DeclRegion's maintain declaration order, the
    /// index represents the zeroth, first, second, ..., declaration of the
    /// given name).  If no matching declaration is found in the region, -1 is
    /// returned.
    static int getDeclIndex(const Decl *decl, const DeclRegion *region);

    /// \brief Returns the name of a subroutine, translating binary operators
    /// into a unique long-form.
    ///
    /// For example, "*" translates into "0multiply" and "+" translates into
    /// "0plus".  This is a helper function for CodeGen::getLinkName.
    static std::string getSubroutineName(const SubroutineDecl *srd);
};

}; // end comma namespace

#endif
