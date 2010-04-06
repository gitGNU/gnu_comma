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
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/System/Path.h"
#include <vector>

namespace comma {

//===----------------------------------------------------------------------===//
// CompilationUnit
//
class CompilationUnit {

public:
    /// Creates a compilation unit associated with the given file.
    CompilationUnit(const llvm::sys::Path &path)
        : path(path) { }

    /// Returns the path name supporting this compilation unit.
    const llvm::sys::Path &getPath() const { return path; }

    /// Registers the given declaration with this compilation unit.
    void addDeclaration(Decl *decl) { declarations.push_back(decl); }

    /// Registers the given declaration as a dependency of this compilation unit.
    ///
    /// Returns true if \p decl was newly inserted into this compilation units
    /// dependency set and false if \p decl was already a member.
    bool addDependency(Decl *decl) { return dependencies.insert(decl); }

    /// Returns true if \p decl is already registered as a dependency.
    bool isDependency(Decl *decl) { return dependencies.count(decl) != 0; }

    //@{
    /// Iterators over the declarations contained in this compilation unit.  The
    /// declarations are returned in the order they were installed via calls to
    /// add_declaration.
    typedef std::vector<Decl *>::const_iterator decl_iterator;
    decl_iterator begin_declarations() const { return declarations.begin(); }
    decl_iterator end_declarations()   const { return declarations.end(); }
    //@}

    //@{
    /// Iterators over the dependency declarations.
    typedef llvm::SmallPtrSet<Decl*, 8>::const_iterator dep_iterator;
    dep_iterator begin_dependencies() const { return dependencies.begin(); }
    dep_iterator end_dependencies() const { return dependencies.end(); }
    //@}

private:
    llvm::sys::Path path;

    std::vector<Decl*> declarations;
    llvm::SmallPtrSet<Decl*, 8> dependencies;
};

} // End comma namespace

#endif
