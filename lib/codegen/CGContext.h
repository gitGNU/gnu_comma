//===-- codegen/CGContext.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENCONTEXT_HDR_GUARD
#define COMMA_CODEGEN_CODEGENCONTEXT_HDR_GUARD

#include "CodeGenTypes.h"
#include "InstanceInfo.h"

#include <map>

namespace comma {

class CodeGenTypes;

/// \class
///
/// \brief An interface to update and retrieve the current state of the code
/// generator.
///
/// A CGContext object is one level beneath the CodeGen class in the sense that
/// it encapsulates transient state (as opposed to the global or module level
/// state that CodeGen provides).
class CGContext {

public:
    /// Constructs a CGContext to encapsulate the top level state associated
    /// with the given InstanceInfo.
    CGContext(CodeGen &CG, InstanceInfo *IInfo);

    //@{
    /// Returns the code generator.
    const CodeGen &getCG() const { return CG; }
    CodeGen &getCG() { return CG; }
    //@}

    /// Returns the Comma to LLVM type generator.
    ///
    /// Note that the supplied type generator is initialized to respect the
    /// current context.  In particular, it will lower generic formal parameters
    /// to the representation of the actual parameter.
    CodeGenTypes &getCGT() { return CGT; }

    /// \name General Predicates.
    //@{
    /// Returns true if we are currently generating a capsule.
    bool generatingCapsule() const { return IInfo != 0; }

    /// Returns true if we are generating an instance of a generic capsule.
    bool generatingCapsuleInstance() const;
    //@}

    //@{
    /// Returns the InstanceInfo object associated with this context, or null if
    /// there is no capsule currently registered.
    const InstanceInfo *getInstanceInfo() const { return IInfo; }
    InstanceInfo *getInstanceInfo() { return IInfo; }
    //@}

    /// \name Generic Parameters.
    ///
    /// The AST is not rewritten in its entirety for every instance of a generic
    /// unit.  The following methods provide access to maps which rewrite
    /// generic formal parameters to their corresponding actuals.
    //@{

    /// The type used to map generic types to the corresponding actuals.
    typedef std::map<Type*, Type*> ParameterMap;

    /// Returns the current parameter map.
    const ParameterMap &getParameterMap() const { return paramMap; }

    //@{
    /// Returns the instance decl corresponding to the given abstract decl using
    /// the current parameter map, or null if there is no association.
    DomainInstanceDecl *
    rewriteAbstractDecl(AbstractDomainDecl *abstract) const;

    const DomainInstanceDecl *
    rewriteAbstractDecl(const AbstractDomainDecl *abstract) const {
        return rewriteAbstractDecl(const_cast<AbstractDomainDecl*>(abstract));
    }
    //@}
    //@}

private:
    CodeGen &CG;
    InstanceInfo *IInfo;
    CodeGenTypes CGT;
    ParameterMap paramMap;
};

} // end comma namespace.

#endif
