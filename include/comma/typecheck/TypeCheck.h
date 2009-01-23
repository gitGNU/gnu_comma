//===-- typecheck/TypeCheck.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_TYPECHECK_HDR_GUARD
#define COMMA_TYPECHECK_TYPECHECK_HDR_GUARD

#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include "comma/parser/Bridge.h"
#include "comma/ast/AstBase.h"
#include "comma/ast/Cunit.h"
#include "comma/ast/Scope.h"
#include "comma/ast/AstResource.h"
#include "llvm/Support/Casting.h"
#include <iosfwd>

namespace comma {

class TypeCheck : public Bridge {

public:
    TypeCheck(Diagnostic      &diag,
              AstResource     &resource,
              CompilationUnit *cunit);

    ~TypeCheck();

    void beginSignatureDefinition(IdentifierInfo *name, Location location);
    void beginDomainDefinition(IdentifierInfo *name, Location location);
    void endModelDefinition();

    // Called immediately after a model definition has been registered.  This
    // call defines a formal parameter of the model (which must be either a
    // functor or variety).
    Node acceptModelParameter(IdentifierInfo *formal, Node type, Location loc);

    // Once parameter parsing is complete, all nodes returned by calls to
    // acceptModelParameter are provided to the type checker.
    void acceptModelParameterList(Node *params, Location *locs, unsigned arity);

    // For each supersignature, this function is called to notify the type
    // checker of the existance of a supersignature.
    Node acceptModelSupersignature(Node typeNode, Location loc);

    // Once the list of supersignatures have been parsed, all nodes returned by
    // calls to acceptModelSupersignature are provided to the type checker.
    void acceptModelSupersignatureList(Node *sigs, unsigned numSigs);

    Node acceptSignatureDecl(IdentifierInfo *name,
                             Node typeNode, Location loc);

    void acceptSignatureDecls(Node    *components,
                              unsigned numComponents);

    void acceptDeclaration(IdentifierInfo *name,
                           Node            type,
                           Location        loc);

    Node acceptPercent(Location loc);

    Node acceptTypeIdentifier(IdentifierInfo *info, Location loc);

    Node acceptTypeApplication(IdentifierInfo  *connective,
                               Node            *arguments,
                               Location        *argumentLocs,
                               unsigned         numArgs,
                               IdentifierInfo **selectors,
                               Location        *selectorLocs,
                               unsigned         numSelectors,
                               Location         loc);

    Node acceptFunctionType(IdentifierInfo **formals,
                            Location        *formalLocations,
                            Node            *types,
                            Location        *typeLocations,
                            unsigned         arity,
                            Node             returnType,
                            Location         returnLocation);

    // Delete the underlying Ast node.
    void deleteNode(Node node);

private:
    Diagnostic &diagnostic;
    AstResource &resource;
    CompilationUnit *compUnit;

    // Lifts a Node to the corresponding Ast type.  If the node is not of the
    // supplied type, this function returns 0.
    template <class T>
    static T *lift(Node &node) {
        return llvm::dyn_cast_or_null<T>(Node::lift<Ast>(node));
    }

    struct ModelInfo {
        Ast::AstKind    kind;
        IdentifierInfo *name;
        Location        location;
        ModelDecl      *decl;

        ModelInfo(Ast::AstKind kind, IdentifierInfo *name, Location loc)
            : kind(kind), name(name), location(loc), decl(0) { }
    };

    ModelInfo *currentModelInfo;

    ModelDecl *getCurrentModel() const {
        return currentModelInfo->decl;
    }

    Sigoid *getCurrentSignature() const;

    DeclarativeRegion *declarativeRegion;

    DeclarativeRegion *currentDeclarativeRegion() {
        return declarativeRegion;
    }

    // The top level scope is a compilation unit scope and never changes during
    // analysis.  The current scope is some inner scope of the top scope and
    // reflects the current state of the analysis.
    Scope *topScope;
    Scope *currentScope;

    unsigned errorCount;

    CompilationUnit *currentCompUnit() const { return compUnit; }

    Scope::ScopeKind currentScopeKind() const {
        return currentScope->getKind();
    }

    void pushModelScope() {
        currentScope = currentScope->pushScope(Scope::MODEL_SCOPE);
    }

    void popModelScope() {
        popScope();
    }

    void popScope() {
        // FIXME: There should be a "scope cache" in place so that malloc
        // traffic is kept to a reasonable minimum.
        Scope *parent = currentScope->popScope();
        delete currentScope;
        currentScope = parent;
    }

    //===------------------------------------------------------------------===//
    // Lookup operations.
    //===------------------------------------------------------------------===//

    void addType(ModelType *type) {
        currentScope->addType(type);
    }

    ModelType *lookupType(IdentifierInfo *info, bool traverse = true) {
        return currentScope->lookupType(info, traverse);
    }

    //===------------------------------------------------------------------===//
    // Utility functions.
    //===------------------------------------------------------------------===//

    void ensureNecessaryRedeclarations(Sigoid *sig);

    DomainType *ensureDomainType(Node typeNode, Location loc) const;

    static SignatureType *resolveArgumentType(ParameterizedType *target,
                                              DomainType **actuals,
                                              unsigned numActuals);

    bool has(DomainType *source, SignatureType *target);

    SourceLocation getSourceLocation(Location loc) const {
        return resource.getTextProvider().getSourceLocation(loc);
    }

    DiagnosticStream &report(Location loc, diag::Kind kind) const {
        return diagnostic.report(getSourceLocation(loc), kind);
    }
};

} // End comma namespace

#endif
