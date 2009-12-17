//===-- ast/ExceptionRef.h ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_EXCEPTIONREF_HDR_GUARD
#define COMMA_AST_EXCEPTIONREF_HDR_GUARD

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the ExceptionRef AST node.
//===----------------------------------------------------------------------===//

#include "comma/ast/AstBase.h"
#include "comma/ast/Decl.h"

namespace comma {

//===----------------------------------------------------------------------===//
// ExceptionRef
//
/// \class
///
/// This class is a simple AST node which wraps an ExceptionDecl together with a
/// location.
class ExceptionRef : public Ast {

public:
    ExceptionRef(Location loc, ExceptionDecl *exception)
        : Ast(AST_ExceptionRef), loc(loc), exception(exception) { }

    /// Returns the location associated with this ExceptionRef.
    Location getLocation() const { return loc; }

    /// Returns the exception declaration this node references.
    //@{
    const ExceptionDecl *getException() const { return exception; }
    ExceptionDecl *getException() { return exception; }
    //@}

    /// Sets the exception associated with this reference.
    void setException(ExceptionDecl *exception) { this->exception = exception; }

    /// Returns the defining identifier of the associated exception declaration.
    IdentifierInfo *getIdInfo() const { return exception->getIdInfo(); }

    /// Returns the defining identifier of the associated exception as a
    /// C-string.
    const char *getString() const { return exception->getString(); }

    // Support isa/dyn_cast.
    static bool classof(const ExceptionRef *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ExceptionRef;
    }

private:
    Location loc;
    ExceptionDecl *exception;
};

} // end comma namespace.

#endif

