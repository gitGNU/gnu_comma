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

namespace llvm {

class APInt;

} // end llvm namespace.

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
    void acceptWithSupersignature(Node typeNode, Location loc);

    void acceptCarrier(IdentifierInfo *name, Node typeNode, Location loc);

    void beginAddExpression();
    void endAddExpression();

    bool acceptObjectDeclaration(Location        loc,
                                 IdentifierInfo *name,
                                 Node            type,
                                 Node            initializer);


    void acceptDeclarationInitializer(Node declNode, Node initializer);

    Node acceptPercent(Location loc);

    Node acceptTypeName(IdentifierInfo *info,
                        Location        loc,
                        Node            qualNode);

    Node acceptTypeApplication(IdentifierInfo  *connective,
                               NodeVector      &arguments,
                               Location        *argumentLocs,
                               IdentifierInfo **keys,
                               Location        *keyLocs,
                               unsigned         numKeys,
                               Location         loc);

    void beginSubroutineDeclaration(Descriptor &desc);

    Node acceptSubroutineParameter(IdentifierInfo *formal,
                                   Location loc,
                                   Node typeNode,
                                   PM::ParameterMode mode);

    Node acceptSubroutineDeclaration(Descriptor &desc,
                                     bool        definitionFollows);

    // Begin a subroutine definition, using a valid node returned from
    // acceptSubroutineDeclaration to establish context.
    void beginSubroutineDefinition(Node declarationNode);

    void acceptSubroutineStmt(Node stmt);

    void endSubroutineDefinition();

    bool acceptImportDeclaration(Node importedType, Location loc);

    Node acceptKeywordSelector(IdentifierInfo *key,
                               Location        loc,
                               Node            exprNode,
                               bool            forSubroutine);

    Node acceptDirectName(IdentifierInfo *name,
                          Location        loc,
                          Node            qualNode);

    Node acceptFunctionName(IdentifierInfo *name,
                            Location        loc,
                            Node            qualNode);

    Node acceptFunctionCall(Node        connective,
                            Location    loc,
                            NodeVector &args);


    Node acceptProcedureName(IdentifierInfo *name,
                             Location        loc,
                             Node            qualNode);

    Node acceptProcedureCall(Node        connective,
                             Location    loc,
                             NodeVector &args);

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

    Node acceptIntegerLiteral(const llvm::APInt &value, Location loc);

    Node acceptIfStmt(Location loc, Node condition,
                      Node *consequents, unsigned numConsequents);

    Node acceptElseStmt(Location loc, Node ifNode,
                        Node *alternates, unsigned numAlternates);

    Node acceptElsifStmt(Location loc,
                         Node     ifNode,
                         Node     condition,
                         Node    *consequents,
                         unsigned numConsequents);

    Node acceptEmptyReturnStmt(Location loc);

    Node acceptReturnStmt(Location loc, Node retNode);

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
    Node beginEnumerationType(IdentifierInfo *name,
                              Location        loc);

    // Called for each literal composing an enumeration type, where the first
    // argument is a valid node as returned by acceptEnumerationType.
    void acceptEnumerationLiteral(Node            enumeration,
                                  IdentifierInfo *name,
                                  Location        loc);
    // Called when all of the enumeration literals have been processed, thus
    // completing the definition of the enumeration.
    void endEnumerationType(Node enumeration);

    /// Called to process integer type definitions.
    ///
    /// For example, given a definition of the form <tt>type T is range
    /// X..Y;</tt>, this callback is invoked with \p name set to the identifier
    /// \c T, \p loc set to the location of \p name, \p low set to the
    /// expression \c X, and \p high set to the expression \c Y.
    void acceptIntegerTypedef(IdentifierInfo *name, Location loc,
                              Node low, Node high);

    // Delete the underlying Ast node.
    void deleteNode(Node &node);

    /// \brief Returns true if the type checker as not encountered an error and
    /// false otherwise.
    bool checkSuccessful() const { return errorCount == 0; }

    /// \brief Returns the number of errors encountered thus far.
    unsigned getErrorCount() const { return errorCount; }

private:
    Diagnostic      &diagnostic;
    AstResource     &resource;
    CompilationUnit *compUnit;
    DeclRegion      *declarativeRegion;
    ModelDecl       *currentModel;
    Scope            scope;
    unsigned         errorCount;

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

    DeclRegion *currentDeclarativeRegion() const {
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
    static SubroutineDecl *makeSubroutineDecl(IdentifierInfo *name,
                                              Location        loc,
                                              SubroutineType *type,
                                              DeclRegion     *region);

    void ensureNecessaryRedeclarations(ModelDecl *model);

    /// Returns true if the given identifier can be used to name a new type
    /// within the context of the given declarative region.  Otherwise, false is
    /// returned and diagnostics are posted with respect to the given location.
    bool ensureDistinctTypeName(IdentifierInfo *name, Location loc,
                                DeclRegion *region);

    void aquireSignatureTypeDeclarations(ModelDecl *model, Sigoid *sigdecl);

    DomainType *ensureDomainType(Node typeNode, Location loc, bool report = true);
    DomainType *ensureDomainType(Type *type, Location loc, bool report = true);
    Type *ensureValueType(Node typeNode, Location loc, bool report = true);
    Type *ensureValueType(Type *type, Location loc, bool report = true);

    bool ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result);

    Node acceptQualifiedName(Node            qualNode,
                             IdentifierInfo *name,
                             Location        loc);

    Expr *resolveDirectDecl(IdentifierInfo *name, Location loc);

    // Resolves the given call expression (which should have multiple candidate
    // connectives) to one which satisfies the given target type and returns
    // true.  Otherwise, false is returned and the appropriate diagnostics are
    // emitted.
    bool resolveFunctionCall(FunctionCallExpr *call, Type *type);

    // Resolves the given call expression (which must be nullary function call,
    // i.e. one without arguments) to one which satisfies the given target type
    // and returns true.  Otherwise, false is returned and the appropriate
    // diagnostics are emitted.
    bool resolveNullaryFunctionCall(FunctionCallExpr *call,
                                    Type             *targetType);

    Node checkSubroutineCall(SubroutineDecl *decl,
                             Location        loc,
                             Expr          **args,
                             unsigned        numArgs);

    /// Checks that the supplied array of arguments are compatible with those of
    /// the given decl.  This is a helper method for checkSubroutineCall.
    ///
    /// It is assumed that the number of arguments passed matches the number
    /// expected by the decl.  This function checks that the argument types and
    /// modes are compatible with that of the given decl.  Returns true if the
    /// check succeeds, false otherwise and appropriate diagnostics are posted.
    bool checkSubroutineArguments(SubroutineDecl *decl,
                                  Expr **args,
                                  unsigned numArgs);

    Node acceptSubroutineCall(IdentifierInfo *name,
                              Location        loc,
                              NodeVector     &args,
                              bool            checkFunction);

    Node acceptSubroutineCall(std::vector<SubroutineDecl*> &decls,
                              Location                      loc,
                              NodeVector                   &args);

    static bool lookupSubroutineDecls(Homonym                      *homonym,
                                      unsigned                      arity,
                                      std::vector<SubroutineDecl*> &routines,
                                      bool lookupFunctions);

    // Looks up all function declarations in the given decl
    static void collectFunctionDecls(IdentifierInfo *name,
                                     unsigned        arity,
                                     DeclRegion     *region,
                                     std::vector<FunctionDecl*> dst);

    // Returns true if the source type is compatible with the target type.  In
    // this case the target denotes a signature, and so the source must be a
    // domain which satisfies the signature constraint.  The supplied location
    // indicates the position of the source type.
    bool checkType(Type *source, SignatureType *target, Location loc);


    // Returns true if an expression satisfies the target type, performing any
    // resolution of the expression as needed.  Otherwise false is returned and
    // an appropriate diagnostics is posted.
    bool checkType(Expr *expr, Type *targetType);

    // Verifies that the given AddDecl satisfies the constraints imposed by its
    // signature.  Returns true if the constraints are satisfied.  Otherwise,
    // false is returned and diagnostics are posted.
    bool ensureExportConstraints(AddDecl *add);

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

    // Search all declarations present in the given declarative region for a
    // match with respect to the given rewrites.  Returns a matching delcaration
    // node or null.
    static SubroutineDecl *findDecl(const AstRewriter &rewrites,
                                    DeclRegion        *region,
                                    SubroutineDecl    *decl);

    bool has(DomainType *source, SignatureType *target);

    SourceLocation getSourceLoc(Location loc) const {
        return resource.getTextProvider().getSourceLocation(loc);
    }

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        ++errorCount;
        return diagnostic.report(getSourceLoc(loc), kind);
    }
};

} // End comma namespace

#endif
