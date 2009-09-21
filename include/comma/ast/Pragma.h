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

namespace comma {

//===----------------------------------------------------------------------===//
// Pragma

class Pragma {

public:
    virtual ~Pragma() { }

    enum PragmaKind {
        Assert
    };

    PragmaKind getKind() const { return kind; }

    Location getLocation() const { return loc; }

    // Support isa and dyn_cast.
    static bool classof(const Pragma *pragma) { return true; }

protected:
    Pragma(PragmaKind kind, Location loc) : kind(kind), loc(loc) { }

    PragmaKind kind;
    Location loc;
};

//===----------------------------------------------------------------------===//
// PragmaAssert

class PragmaAssert : public Pragma {

public:
    PragmaAssert(Location loc, Expr *condition, const std::string &message)
        : Pragma(Pragma::Assert, loc),
          condition(condition),
          message(message) { }

    const Expr *getCondition() const { return condition; }
    Expr *getCondition() { return condition; }

    const std::string &getMessage() const { return message; }

    // Support isa and dyn_cast.
    static bool classof(const PragmaAssert *pragma) { return true; }
    static bool classof(const Pragma *pragma) {
        return pragma->getKind() == Pragma::Assert;
    }

private:
    Expr *condition;
    std::string message;
};

} // end comma namespace.

#endif
