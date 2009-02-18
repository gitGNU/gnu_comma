//===-- typecheck/TypeCheck.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_TYPECHECK_HDR_GUARD
#define COMMA_TYPECHECK_TYPECHECK_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Cunit.h"
#include "comma/ast/AstResource.h"
#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include "comma/parser/ParseClient.h"
#include "comma/typecheck/Scope.h"
#include "llvm/Support/Casting.h"
#include <iosfwd>

namespace comma {

class TypeCheck : public ParseClient {

public:
    TypeCheck(Diagnostic      &diag,
              AstResource     &resource,
              CompilationUnit *cunit);

    ~TypeCheck();

    void beginModelDeclaration(Descriptor &desc);
    void endModelDefinition();

    // Called immediately after a model declaration has been registered.  This
    // call defines a formal parameter of a model.  The parser collects the
    // results of this call into a Descriptor object and supplies them back to
    // the client in a call to acceptModelDeclaration.
    Node acceptModelParameter(IdentifierInfo *formal, Node type, Location loc);

    // This call completes the declaration of a model (name and
    // parameterization).
    void acceptModelDeclaration(Descriptor &desc);

    void beginWithExpression();
    void endWithExpression();

    // Called for each supersignature in a with expression.
    Node acceptWithSupersignature(Node typeNode, Location loc);

    void beginAddExpression();
    void endAddExpression();

    Node acceptDeclaration(IdentifierInfo *name,
                           Node            type,
                           Location        loc);

    void acceptDeclarationInitializer(Node declNode, Node initializer);

    Node acceptPercent(Location loc);

    Node acceptTypeIdentifier(IdentifierInfo *info, Location loc);

    Node acceptTypeApplication(IdentifierInfo  *connective,
                               Node            *arguments,
                               Location        *argumentLocs,
                               unsigned         numArgs,
                               IdentifierInfo **keys,
                               Location        *keyLocs,
                               unsigned         numKeys,
                               Location         loc);

    Node acceptFunctionType(IdentifierInfo **formals,
                            Location        *formalLocations,
                            Node            *types,
                            Location        *typeLocations,
                            unsigned         arity,
                            Node             returnType,
                            Location         returnLocation);

    void beginSubroutineDeclaration(Descriptor &desc);

    Node acceptSubroutineParameter(IdentifierInfo   *formal,
                                   Location          loc,
                                   Node              typeNode,
                                   ParameterMode     mode);

    Node acceptSubroutineDeclaration(Descriptor &desc,
                                     bool        definitionFollows);

    // Begin a subroutine definition, using a valid node returned from
    // acceptSubroutineDeclaration to establish context.
    void beginSubroutineDefinition(Node declarationNode);
    void endSubroutineDefinition();

    void acceptImportStatement(Node importedType, Location loc);

    Node acceptKeywordSelector(IdentifierInfo *key,
                               Location        loc,
                               Node            exprNode,
                               bool            forSubroutine);

    Node acceptDirectName(IdentifierInfo *name, Location loc);

    Node acceptFunctionCall(IdentifierInfo  *name,
                            Location         loc,
                            Node            *args,
                            unsigned         numArgs);

    Node acceptProcedureCall(IdentifierInfo  *name,
                             Location         loc,
                             Node            *args,
                             unsigned         numArgs);

    // Delete the underlying Ast node.
    void deleteNode(Node node);

private:
    Diagnostic      &diagnostic;
    AstResource     &resource;
    CompilationUnit *compUnit;

    // Converts a Node to the corresponding Ast type.  If the node is not of the
    // supplied type, this function returns 0.
    template <class T>
    static T *lift_node(Node &node) {
        return llvm::dyn_cast_or_null<T>(Node::lift<Ast>(node));
    }

    // Casts the given Node to the corresponding type and asserts that the
    // conversion was successful.
    template <class T>
    static T *cast_node(Node &node) {
        return llvm::cast<T>(Node::lift<Ast>(node));
    }

    ModelDecl *currentModel;

    ModelDecl *getCurrentModel() const {
        return currentModel;
    }

    Sigoid *getCurrentSignature() const;

    Domoid *getCurrentDomain() const;

    DeclarativeRegion *declarativeRegion;

    DeclarativeRegion *currentDeclarativeRegion() {
        return declarativeRegion;
    }

    // The top level scope is a compilation unit scope and never changes during
    // analysis.  The current scope is some inner scope of the top scope and
    // reflects the current state of the analysis.
    Scope scope;

    unsigned errorCount;

    CompilationUnit *currentCompUnit() const { return compUnit; }

    //===------------------------------------------------------------------===//
    // Utility functions.
    //===------------------------------------------------------------------===//

    // Creates a procedure or function decl depending on the kind of the
    // supplied type.
    static SubroutineDecl *makeSubroutineDecl(IdentifierInfo    *name,
                                              Location           loc,
                                              SubroutineType    *type,
                                              DeclarativeRegion *region);

    void ensureNecessaryRedeclarations(ModelDecl *model);

    DomainType *ensureDomainType(Node typeNode, Location loc) const;

    static SignatureType *resolveArgumentType(ParameterizedType *target,
                                              DomainType **actuals,
                                              unsigned numActuals);

    bool resolveDirectDecl(Decl           *candidate,
                           IdentifierInfo *name,
                           Location        loc,
                           Node           &node);

    bool resolveFunctionCall(FunctionCallExpr *call, Type *type);

    Node checkSubroutineCall(SubroutineDecl *decl,
                             Location        loc,
                             Expr          **args,
                             unsigned        numArgs);


    Node acceptSubroutineCall(IdentifierInfo *name,
                              Location        loc,
                              Node           *args,
                              unsigned        numArgs,
                              bool            checkFunction);

    static void lookupSubroutineDecls(Homonym *homonym,
                                      unsigned arity,
                                      llvm::SmallVector<SubroutineDecl*, 8> &routines,
                                      bool lookupFunctions);

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
