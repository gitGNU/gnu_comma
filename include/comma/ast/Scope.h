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

    // Adds the given type into the type namespace.  If a type with the same
    // name already exists in this scope, this method will assert.
    void addModel(ModelDecl *decl);

    // Adds the given type indo the type namespace, registered under the given
    // IdentifierInfo.
    void addModel(IdentifierInfo *info, ModelDecl *decl);

    // Looks up the given model.  If traverse is true, the lookup includes all
    // parent scopes, otherwise the lookup is constrained to this scope.
    ModelDecl *lookupModel(const IdentifierInfo *info, bool traverse = true) const;

private:
    // Internal constructor for creating scopes of arbitrary kinds.
    Scope(ScopeKind kind, Scope *parent);

    ScopeKind kind;
    Scope *parentScope;
    Scope *childScope;

    // The set of declaration nodes which this scope provides.
    std::vector<Decl *> declarations;

    // Installed in the metadata slot of Identifier_Info's.  This structure
    // stores the nodes which correspond to the various namespaces.
    struct DeclInfo {
        DeclInfo(Scope *scope) : scope(scope) { }

        // Tag indicating which scope this entry belongs to.
        Scope *scope;

        // Only one type with a given name can exist in a scope at once.
        TypeDecl *type;
    };

    typedef std::vector<DeclInfo> DeclStack;

    void modifyDeclStack(IdentifierInfo *idInfo, ModelDecl *decl);

    void ensureDisjointDeclarations() const;
};

} // End comma namespace

#endif
