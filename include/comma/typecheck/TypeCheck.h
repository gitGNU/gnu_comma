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
    // results of this call into the given descriptor object (and hense, the
    // supplied descriptor contains all previously accumulated arguments) to be
    // finalized in a call to acceptModelDeclaration.
    Node acceptModelParameter(Descriptor     &desc,
                              IdentifierInfo *formal,
                              Node            type,
                              Location        loc);

    // This call completes the declaration of a model (name and
    // parameterization).
    void acceptModelDeclaration(Descriptor &desc);

    void beginWithExpression();
    void endWithExpression();

    // Called for each supersignature in a with expression.
    Node acceptWithSupersignature(Node typeNode, Location loc);

    void acceptCarrier(IdentifierInfo *name, Node typeNode, Location loc);

    void beginAddExpression();
    void endAddExpression();

    Node acceptObjectDeclaration(Location        loc,
                                 IdentifierInfo *name,
                                 Node            type,
                                 Node            initializer);


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

    void acceptSubroutineStmt(Node stmt);

    void endSubroutineDefinition();

    Node acceptImportDeclaration(Node importedType, Location loc);

    Node acceptKeywordSelector(IdentifierInfo *key,
                               Location        loc,
                               Node            exprNode,
                               bool            forSubroutine);

    Node acceptDirectName(IdentifierInfo *name, Location loc);

    Node acceptQualifiedName(Node            qualifier,
                             IdentifierInfo *name,
                             Location        loc);

    Node acceptFunctionCall(IdentifierInfo  *name,
                            Location         loc,
                            Node            *args,
                            unsigned         numArgs);

    Node acceptProcedureCall(IdentifierInfo  *name,
                             Location         loc,
                             Node            *args,
                             unsigned         numArgs);

    // Called for "inj" expressions.  loc is the location of the inj token and
    // expr is its argument.
    Node acceptInj(Location loc, Node expr);

    // Called for "prj" expressions.  loc is the location of the prj token and
    // expr is its argument.
    Node acceptPrj(Location loc, Node expr);

    Node acceptQualifier(Node qualifierType, Location loc);

    Node acceptNestedQualifier(Node     qualifier,
                               Node     qualifierType,
                               Location loc);

    Node acceptIfStmt(Location loc, Node condition,
                      Node *consequents, unsigned numConsequents);

    Node acceptElseStmt(Location loc, Node ifNode,
                        Node *alternates, unsigned numAlternates);

    Node acceptElsifStmt(Location loc,
                         Node     ifNode,
                         Node     condition,
                         Node    *consequents,
                         unsigned numConsequents);

    Node acceptReturnStmt(Location loc, Node retNode = 0);

    Node acceptAssignmentStmt(Location        loc,
                              IdentifierInfo *target,
                              Node            value);

    // Called when a block statement is about to be parsed.
    Node beginBlockStmt(Location loc, IdentifierInfo *label = 0);

    // This method is called for each statement associated with the block.
    void acceptBlockStmt(Node block, Node stmt);

    // Once the last statement of a block has been parsed, this method is called
    // to inform the client that we are leaving the block context established by
    // the last call to beginBlockStmt.
    void endBlockStmt(Node block);

    // Called when an enumeration type is about to be parsed, supplying the name
    // of the type and its location.  For each literal composing the
    // enumeration, acceptEnumerationLiteral is called with the result of this
    // function.
    Node acceptEnumerationType(IdentifierInfo   *name,
                                       Location loc);

    // Called for each literal composing an enumeration type, where the first
    // argument is a valid node as returned by acceptEnumerationType.
    void acceptEnumerationLiteral(Node            enumeration,
                                  IdentifierInfo *name,
                                  Location        loc);

    // Delete the underlying Ast node.
    void deleteNode(Node node);

private:
    Diagnostic        &diagnostic;
    AstResource       &resource;
    CompilationUnit   *compUnit;
    DeclarativeRegion *declarativeRegion;
    ModelDecl         *currentModel;
    Scope              scope;
    unsigned           errorCount;

    // FIXME: We need a class to hold instances of primitive types.  For now, we
    // simply stash them inline.
    EnumerationDecl *theBoolDecl;

    //===------------------------------------------------------------------===//
    // Utility functions.
    //===------------------------------------------------------------------===//

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

    DeclarativeRegion *currentDeclarativeRegion() const {
        return declarativeRegion;
    }

    CompilationUnit *currentCompUnit() const { return compUnit; }

    ModelDecl *getCurrentModel() const {
        return currentModel;
    }

    // If we are processing a Sigoid, return the current model cast to a
    // Sigoid, else 0.
    Sigoid *getCurrentSigoid() const;

    // If we are processing a Signature, return the current model cast to a
    // SignatureDecl, else 0.
    SignatureDecl *getCurrentSignature() const;

    // If we are processing a Variety, return the current model cast to a
    // VarietyDecl, else 0.
    VarietyDecl *getCurrentVariety() const;

    // If we are processing a Domoid, return the current model cast to a
    // Domoid, else 0.
    Domoid *getCurrentDomoid() const;

    // If we are processing a Domain, return the current model cast to a
    // DomainDecl, else 0.
    DomainDecl *getCurrentDomain() const;

    // If we are processing a Functor, return the current model cast to a
    // FunctorDecl, else 0.
    FunctorDecl *getCurrentFunctor() const;

    // If we are processing a subroutine, return the current SubroutineDecl,
    // else 0.
    SubroutineDecl *getCurrentSubroutine() const;

    // If we are processing a procedure, return the current ProcedureDecl, else
    // 0.
    ProcedureDecl *getCurrentProcedure() const;

    // If we are processing a function, return the current FunctionDecl, else 0.
    FunctionDecl *getCurrentFunction() const;

    // Returns the % node for the current model, or 0 if we are not currently
    // processing a model.
    DomainType *getCurrentPercent() const;

    // Returns true if we are currently checking a domain.
    bool checkingDomain() const { return getCurrentDomain() != 0; }

    // Returns true if we are currently checking a functor.
    bool checkingFunctor() const { return getCurrentFunctor() != 0; }

    // Returns true if we are currently checking a signature.
    bool checkingSignature() const { return getCurrentSignature() != 0; }

    // Returns true if we are currently checking a variety.
    bool checkingVariety() const { return getCurrentVariety() != 0; }

    // Returns true if we are currently checking a procedure.
    bool checkingProcedure() const { return getCurrentProcedure() != 0; }

    // Returns true if we are currently checking a function.
    bool checkingFunction() const { return getCurrentFunction() != 0; }

    // Called when then type checker is constructed.  Populates the top level
    // scope with an initial environment.
    void populateInitialEnvironment();

    // Creates a procedure or function decl depending on the kind of the
    // supplied type.
    static SubroutineDecl *makeSubroutineDecl(IdentifierInfo    *name,
                                              Location           loc,
                                              SubroutineType    *type,
                                              DeclarativeRegion *region);

    void ensureNecessaryRedeclarations(ModelDecl *model);
    bool ensureDistinctTypeDeclaration(DeclarativeRegion *region,
                                       TypeDecl          *tyDecl);
    void aquireSignatureTypeDeclarations(ModelDecl *model, Sigoid *sigdecl);

    DomainType *ensureDomainType(Node typeNode, Location loc) const;
    DomainType *ensureDomainType(Type *type, Location loc) const;

    Type *ensureValueType(Node typeNode, Location loc) const;

    static SignatureType *resolveArgumentType(ParameterizedType *target,
                                              Type             **actuals,
                                              unsigned           numActuals);

    Node resolveDirectDecl(Decl           *candidate,
                           IdentifierInfo *name,
                           Location        loc);

    // Resolves the given call expression (which should have multiple candidate
    // connectives) to one which satisfies the given target type and returns
    // true.  Otherwise, false is returned and the appropriated diagnostics are
    // emitted.
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

    // Returns true if the given type decl is equivalent to % in the context of
    // the current domain.
    bool denotesDomainPercent(const TypeDecl *tyDecl);

    // Returns true if we are currently checking a functor, and if the given
    // functor declaration together with the provided arguments would denote an
    // instance which is equivalent to % in the current context.  For example,
    // given:
    //
    //   domain F (X : T) with
    //      procedure Foo (A : F(X));
    //      ...
    //
    // Then "F(X)" is equivalent to %.  More generally, a functor F applied to
    // its formal arguments in the body of F is equivalent to %.
    //
    // This function assumes that the number and types of the supplied arguments
    // are compatible with the given functor.
    bool denotesFunctorPercent(const FunctorDecl *functor,
                               Type **args, unsigned numArgs);

    // Returns true if an expression satisfies the target type, performing any
    // resolution of the expression as needed.  Otherwise false is returned an
    // appropriate diagnostics are posted.
    bool ensureExprType(Expr *expr, Type *targetType);

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
