//===-- ast/Scope.h ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_SCOPE_HDR_GUARD
#define COMMA_AST_SCOPE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <vector>

namespace comma {

class Scope {

public:
    enum ScopeKind {
        CUNIT_SCOPE,            // compilation unit scope.
        MODEL_SCOPE,            // signature/domain etc, scope.
        FUNCTION_SCOPE          // function scope.
    };

    // Creates an initial compilation unit scope.
    Scope();

    // Returns the kind of this scope.
    ScopeKind getKind() const { return kind; }

    // Returns a scope of the given kind which is a child of this one.
    Scope *pushScope(ScopeKind kind);

    // Returns the parent scope and unlinks all declarations associated with
    // this scope.
    Scope *popScope();

    void addModel(ModelDecl *model);

    // Looks up a model with the given name.  If traverse is true, the lookup
    // includes all parent scopes, otherwise the lookup is constrained to this
    // scope.
    ModelDecl *lookupModel(const IdentifierInfo *info, bool traverse = true) const;

private:
    // Internal constructor for creating scopes of arbitrary kinds.
    Scope(ScopeKind kind, Scope *parent);

    ScopeKind kind;
    Scope *parentScope;
    Scope *childScope;

    // The set of identifiers which this scope provides bindings for.
    typedef llvm::SmallPtrSet<IdentifierInfo*, 16> IdInfoSet;
    IdInfoSet identifiers;

    // Installed in the metadata slot of IdentifierInfo's.  This structure
    // stores the nodes which correspond to the various namespaces.
    struct DeclInfo {
        DeclInfo(Scope *scope) : scope(scope), model(0) { }

        // Tag indicating which scope this entry belongs to.
        Scope *scope;

        // Model declaration associated with this entry.
        ModelDecl *model;
    };

    typedef std::vector<DeclInfo> DeclStack;

    DeclInfo *lookupDeclInfo(IdentifierInfo *idInfo);
    void ensureDisjointDeclarations() const;
};

} // End comma namespace

#endif
