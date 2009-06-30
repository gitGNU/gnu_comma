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

#include <string>

namespace comma {

class CodeGen {

public:
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

private:
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
