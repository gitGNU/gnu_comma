//===-- ast/Pragma.h ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Definitions for the various types of pragmas.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_PRAGMA_HDR_GUARD
#define COMMA_AST_PRAGMA_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/basic/Pragmas.h"

#include "llvm/ADT/ilist.h"
#include "llvm/ADT/StringRef.h"

namespace comma {

//===----------------------------------------------------------------------===//
// Pragma
class Pragma {

public:
    virtual ~Pragma() { }

    pragma::PragmaID getKind() const { return ID; }

    Location getLocation() const { return loc; }

    //@{
    /// Pramas may be tied together into doubly linked list structures if needed
    /// using the following methods.
    Pragma *getNext() { return nextLink; }
    Pragma *getPrev() { return prevLink; }

    const Pragma *getNext() const { return nextLink; }
    const Pragma *getPrev() const { return prevLink; }

    void setNext(Pragma *P) { nextLink = P; }
    void setPrev(Pragma *P) { prevLink = P; }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const Pragma *pragma) { return true; }

protected:
    Pragma(pragma::PragmaID ID, Location loc)
        : ID(ID), loc(loc), nextLink(0), prevLink(0) { }

    /// Default pragma constructor is used to support the generation of
    /// llvm::iplist sentinal nodes.  Such default constructed pragma nodes are
    /// invalid.
    Pragma() : ID(pragma::UNKNOWN_PRAGMA), loc(0), nextLink(0), prevLink(0) { }
    friend class llvm::ilist_sentinel_traits<Pragma>;

    pragma::PragmaID ID;
    Location loc;
    Pragma *nextLink;
    Pragma *prevLink;
};

//===----------------------------------------------------------------------===//
// PragmaAssert
class PragmaAssert : public Pragma {

public:
    PragmaAssert(Location loc, Expr *condition, Expr *message = 0)
        : Pragma(pragma::Assert, loc),
          condition(condition),
          message(message) { }

    //@{
    /// Returns the condition this assertion tests.
    const Expr *getCondition() const { return condition; }
    Expr *getCondition() { return condition; }
    //@}

    //@{
    /// Returns the message associated with this assertion or null if there is
    /// no such message.
    const Expr *getMessage() const { return message; }
    Expr *getMessage() { return message; }
    //@}

    /// Returns true if this assertion has been associated with a message.
    bool hasMessage() const { return message != 0; }

    /// Sets the condition of this assertion.
    void setCondition(Expr *condition) { this->condition = condition; }

    /// Sets the message of this assertion.
    void setMessage(Expr *message) { this->message = message; }

    // Support isa and dyn_cast.
    static bool classof(const PragmaAssert *pragma) { return true; }
    static bool classof(const Pragma *pragma) {
        return pragma->getKind() == pragma::Assert;
    }

private:
    Expr *condition;
    Expr *message;
};

//===----------------------------------------------------------------------===//
// PragmaImport
class PragmaImport : public Pragma {

public:
    /// Enumeration of the conventions supported by an import pragma.
    /// UNKNOWN_CONVENTION is a special marker which does not map to an actual
    /// convention name.
    enum Convention {
        UNKNOWN_CONVENTION,
        C
    };

    PragmaImport(Location loc, Convention convention,
                 IdentifierInfo *entity, Expr *externalName);

    /// Returns the convention which the given string maps to, or
    /// UNKNOWN_CONVENTION if there is no mapping.
    static Convention getConventionID(llvm::StringRef &ref);

    /// Returns the convention associated with this pragma.
    Convention getConvention() const { return convention; }

    /// Returns the identifier naming the entity associated with this pragma.
    IdentifierInfo *getEntityIdInfo() const { return entity; }

    //@{
    /// Returns the expression designating the external name of the entity.
    /// The expression is always of type String.
    Expr *getExternalNameExpr() { return externalNameExpr; }
    const Expr *getExternalNameExpr() const { return externalNameExpr; }
    //@}

    /// Returns a string representation of the external name.
    const std::string &getExternalName() const { return externalName; }

private:
    Convention convention;
    IdentifierInfo *entity;
    Expr *externalNameExpr;
    std::string externalName;
};

} // end comma namespace.

#endif
