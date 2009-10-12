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

namespace comma {

//===----------------------------------------------------------------------===//
// Pragma
class Pragma {

public:
    virtual ~Pragma() { }

    pragma::PragmaID getKind() const { return ID; }

    Location getLocation() const { return loc; }

    // Support isa and dyn_cast.
    static bool classof(const Pragma *pragma) { return true; }

protected:
    Pragma(pragma::PragmaID ID, Location loc) : ID(ID), loc(loc) { }

    pragma::PragmaID ID;
    Location loc;
};

//===----------------------------------------------------------------------===//
// PragmaAssert
class PragmaAssert : public Pragma {

public:
    PragmaAssert(Location loc, Expr *condition, const std::string &message)
        : Pragma(pragma::Assert, loc),
          condition(condition),
          message(message) { }

    const Expr *getCondition() const { return condition; }
    Expr *getCondition() { return condition; }

    const std::string &getMessage() const { return message; }

    // Support isa and dyn_cast.
    static bool classof(const PragmaAssert *pragma) { return true; }
    static bool classof(const Pragma *pragma) {
        return pragma->getKind() == pragma::Assert;
    }

private:
    Expr *condition;
    std::string message;
};

} // end comma namespace.

#endif
