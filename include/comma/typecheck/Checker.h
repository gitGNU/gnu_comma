//===-- typecheck/Checker.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file provides access to the Comma type checker.
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_CHECKER_HDR_GUARD
#define COMMA_TYPECHECK_CHECKER_HDR_GUARD

#include "comma/parser/ParseClient.h"

namespace comma {

class AstResource;
class CompilationUnit;
class Diagnostic;

/// A thin wrapper around ParseClient yielding additional methods specific to
/// semantic analysis.
class Checker : public ParseClient {

public:
    /// \brief Returns true if this type checker has not encountered an error
    /// and false otherwise.
    virtual bool checkSuccessful() const = 0;

    /// Returns the CompilationUnit associated with this checker.
    virtual CompilationUnit *getCompilationUnit() const = 0;

    /// Constructs a type checker.
    static Checker *create(Diagnostic &diag, AstResource &resource,
                           CompilationUnit *cunit);

private:
    Checker(const Checker &checker);             // Do not implement.
    Checker &operator =(const Checker &checker); // Likewise.

protected:
    Checker() { }               // Construct via subclass only.
};

} // end comma namespace.

#endif
