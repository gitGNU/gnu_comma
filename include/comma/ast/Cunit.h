//===-- ast/Cunit.h ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_CUNIT_HDR_GUARD
#define COMMA_AST_CUNIT_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "llvm/System/Path.h"
#include <iosfwd>
#include <vector>

namespace comma {

//===----------------------------------------------------------------------===//
// CompilationUnit
//
class CompilationUnit {

public:
    // Creates a compilation unit associated with the given file.
    CompilationUnit(const llvm::sys::Path &path)
        : path(path) { }

    // Returns the path name supporting this compilation unit.
    const llvm::sys::Path &getPath() const { return path; }

    // Registers the given declaration with this compilation unit.
    void addDeclaration(NamedDecl *decl) {
        declarations.push_back(decl);
    }

    // Iterators over the declarations contained in this compilation unit.  The
    // declarations are returned in the order they were installed via calls to
    // add_declaration.
    typedef std::vector<NamedDecl *>::const_iterator decl_iterator;
    decl_iterator beginDeclarations() const { return declarations.begin(); }
    decl_iterator endDeclarations()   const { return declarations.end(); }

private:
    llvm::sys::Path path;

    std::vector<NamedDecl *> declarations;
};

} // End comma namespace

#endif
