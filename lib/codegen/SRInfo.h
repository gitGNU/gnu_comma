//===-- codegen/SRInfo.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Interface to Comma subroutine declarations and a corresponding LLVM
/// function.
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_SRINFO_HDR_GUARD
#define COMMA_CODEGEN_SRINFO_HDR_GUARD

#include "CodeGen.h"
#include "comma/ast/Decl.h"

#include "llvm/ADT/StringRef.h"

namespace comma {

class SRInfo {

public:
    /// \name Canonical Subroutines
    ///
    /// Currently, all subroutines are declared within the context of a domain.
    /// DomainInstanceDecl's provide a public view of a domains declarations,
    /// whereas PercentDecl's and AddDecl's provide an internal view.  All
    /// publicly visible declarations have a corresponding internal declaration.
    /// The "canonical" declaration is an internal declaration resolved to a
    /// completion (definition) when possible.  Consider the following:
    ///
    /// \code
    ///   domain D with
    ///      procedure Foo;
    ///      procedure Bar;
    ///   add
    ///      procedure Foo is ... end Foo;
    ///      pragma Import(C, Bar, "bar_impl");
    ///   end D;
    /// \endcode
    ///
    /// The public view of \c D is represented by a unique DomainInstanceDecl
    /// which provides declarations for \c Foo and \c Bar.  These public decls
    /// have a corresponding internal representation in the unique PercentDecl
    /// associated with \c D.  The declaration for Foo has a completion
    /// (definition) is \c D's AddDecl -- this is the "canonical" declaration
    /// for \c Foo.  The declaration for Bar does not have a completion in the
    /// AddDecl since it is imported -- the "canonical" declaration is the one
    /// provided by \c D's PercentDecl.
    ///
    /// \see InstanceInfo::getKeySRDecl()

    //@{
    /// Returns the \em canonical SubroutineDecl associated with this info.
    SubroutineDecl *getDeclaration() { return srDecl; }
    const SubroutineDecl *getDeclaration() const { return srDecl; }
    //@}

    /// Returns the link name associated with this SRInfo.
    llvm::StringRef getLinkName() const { return llvmFn->getName(); }

    /// Returns the LLVM Function implementing the subroutine.
    llvm::Function *getLLVMFunction() const { return llvmFn; }

    /// Returns the LLVM Module this subroutine is defined/declared in.
    llvm::Module *getLLVMModule() const { return llvmFn->getParent(); }

    /// Returns true if this info corresponds to a Comma function.
    bool isaFunction() const { return llvm::isa<FunctionDecl>(srDecl); }

    /// Returns true if this info corresponds to a Comma procedure.
    bool isaProcedure() const { return llvm::isa<ProcedureDecl>(srDecl); }

    /// \brief Returns true if this subroutine follows the structure return
    /// calling convention.
    bool hasSRet() const { return llvmFn->hasStructRetAttr(); }

    /// \brief Returns true if this subroutine uses the vstack for its return
    /// value(s).
    bool usesVRet() const {
        // If this is a function returning void but does not use the sret
        // convention, then the value must be returned on the vstack.
        //
        // FIXME: llvm-2.6-svn and up has Type::isVoidTy predicate.  Use it.
        if (isaFunction() &&
            (llvmFn->getReturnType()->getTypeID() == llvm::Type::VoidTyID))
            return !hasSRet();
        return false;
    }

    /// Returns true if this function has an aggregate return value.
    bool hasAggRet() const { return hasSRet() || usesVRet(); }

    /// \brief Returns true if this subroutine is imported from an external
    /// library.
    ///
    /// In particular, this method returns true if the subroutine declaration is
    /// associated with an import pragma.
    bool isImported() const { return srDecl->hasPragma(pragma::Import); }

private:
    /// SRInfo objects may be constructed by InstanceInfo's only.
    SRInfo(SubroutineDecl *decl, llvm::Function *fn)
        : srDecl(decl), llvmFn(fn) { }
    friend class InstanceInfo;

    SubroutineDecl *srDecl;
    llvm::Function *llvmFn;
};

} // end comma namespace.

#endif
