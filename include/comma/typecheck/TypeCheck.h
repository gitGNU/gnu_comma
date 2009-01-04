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
#include "llvm/Support/Casting.h"
#include <iosfwd>

namespace comma {

class TypeCheck : public Bridge {

public:
    TypeCheck(Diagnostic &diag,
              TextProvider &tp,
              CompilationUnit *cunit);

    ~TypeCheck();

    // Processing of models begin with a call to beginModelDefinition,
    // and completed with a call to endModelDefinition.
    void beginModelDefinition(DefinitionKind kind,
                              IdentifierInfo *id, Location location);
    void endModelDefinition();

    // Called immediately after a model definition has been registered.  This
    // call defines a formal parameter of the model (which must be either a
    // functor or variety). The order of the calls determines the shape of the
    // models formal parameter list.
    void acceptModelParameter(IdentifierInfo * formal,
                              Node type, Location loc);

    // Once the formal parameters have been accepted, this function is called to
    // begin processing of a models supersignature list.
    void beginModelSupersignatures();

    // For each supersignature, this function is called to notify the type
    // checker of the existance of a supersignature.
    void acceptModelSupersignature(Node type, const Location loc);

    // Called immediately after all supersignatures have been processed.
    void endModelSupersignatures();

    Node acceptPercent(Location loc);

    Node acceptTypeIdentifier(IdentifierInfo *info, Location loc);

    Node acceptTypeApplication(IdentifierInfo *connective,
                               Node *arguments, unsigned numArgs,
                               Location loc);

    // Delete the underlying Ast node.
    void deleteNode(Node node);

private:
    Diagnostic &diagnostic;
    TextProvider &txtProvider;
    CompilationUnit *compUnit;

    // Lifts a Node to the corresponding Ast type.  If the node is not of the
    // supplied type, this function returns 0.
    template <class T>
    static T *lift(Node &node) {
        return llvm::dyn_cast_or_null<T>(Node::lift<Ast>(node));
    }

    // The top level scope is a compilation unit scope and never changes during
    // analysis.  The current scope is some inner scope of the top scope and
    // reflects the current state of the analysis.
    Scope *topScope;
    Scope *currentScope;

    ModelDecl *currentModel;

    unsigned errorCount;

    CompilationUnit *currentCompUnit() const { return compUnit; }

    Scope::ScopeKind currentScopeKind() const {
        return currentScope->getKind();
    }

    void pushModelScope(ModelDecl *model) {
        currentScope = currentScope->pushScope(Scope::MODEL_SCOPE);
        currentModel = model;
    }

    void popModelScope() {
        // FIXME: This function should restore the model context that existed
        // before the corresponding call to pushModelScope().  For now this is
        // not an issue as we do not support nested model definitions.
        popScope();
        currentModel = 0;
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

    void addModel(ModelDecl *model) {
        currentScope->addModel(model);
    }

    ModelDecl *lookupModel(IdentifierInfo *info) {
        return currentScope->lookupModel(info);
    }

    //===------------------------------------------------------------------===//
    // Utility functions.
    //===------------------------------------------------------------------===//

    bool checkDuplicateFormalParameters(
        const std::vector<IdentifierInfo *>& formals,
        const std::vector<Location> &formalLocations);

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }
};

} // End comma namespace

#endif
