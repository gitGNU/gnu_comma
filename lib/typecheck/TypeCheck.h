//===-- typecheck/TypeCheck.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_TYPECHECK_HDR_GUARD
#define COMMA_TYPECHECK_TYPECHECK_HDR_GUARD


#include "Scope.h"
#include "Stencil.h"
#include "comma/ast/AstBase.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Cunit.h"
#include "comma/ast/Type.h"
#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include "comma/typecheck/Checker.h"

#include "llvm/Support/Casting.h"

#include <stack>

namespace llvm {

class APInt;

} // end llvm namespace.

namespace comma {

class TypeCheck : public Checker {

public:
    TypeCheck(Diagnostic      &diag,
              AstResource     &resource,
              CompilationUnit *cunit);

    ~TypeCheck();

    /// \name ParseClient Requirements.
    ///
    /// \brief Declaration of the ParseClient interface.
    ///
    /// \see ParseClient.
    //@{
    void beginCapsule();
    void endCapsule();

    void beginGenericFormals();
    void endGenericFormals();

    void acceptFormalDomain(IdentifierInfo *name, Location loc, Node sig);

    void beginDomainDecl(IdentifierInfo *name, Location loc);
    void beginSignatureDecl(IdentifierInfo *name, Location loc);

    void beginSignatureProfile();
    void endSignatureProfile();

    void acceptSupersignature(Node typeNode);

    void beginAddExpression();
    void endAddExpression();

    void acceptCarrier(IdentifierInfo *name, Location loc, Node typeNode);

    void beginFunctionDeclaration(IdentifierInfo *name, Location loc);
    void beginProcedureDeclaration(IdentifierInfo *name, Location loc);


    void acceptSubroutineParameter(IdentifierInfo *formal, Location loc,
                                   Node typeNode, PM::ParameterMode mode);

    void acceptFunctionReturnType(Node typeNode);

    Node endSubroutineDeclaration(bool definitionFollows);

    Node beginSubroutineDefinition(Node declarationNode);
    void endSubroutineBody(Node contextNode);
    void endSubroutineDefinition();

    Node acceptDirectName(IdentifierInfo *name, Location loc,
                          bool forStatement);

    Node acceptCharacterLiteral(IdentifierInfo *lit, Location loc);

    Node acceptSelectedComponent(Node prefix,
                                 IdentifierInfo *name,
                                 Location loc,
                                 bool forStatement);

    Node acceptParameterAssociation(IdentifierInfo *key,
                                    Location loc, Node rhs);

    Node acceptApplication(Node prefix, NodeVector &argNodes);

    Node acceptAttribute(Node prefix, IdentifierInfo *name, Location loc);

    Node finishName(Node name);

    void beginAggregate(Location loc);
    void acceptPositionalAggregateComponent(Node component);
    Node acceptAggregateKey(Node lower, Node upper);
    Node acceptAggregateKey(IdentifierInfo *name, Location loc);
    Node acceptAggregateKey(Node key);
    void acceptKeyedAggregateComponent(NodeVector &keys,
                                       Node expr, Location loc);
    void acceptAggregateOthers(Location loc, Node component);
    Node endAggregate();

    Node beginForStmt(Location loc, IdentifierInfo *iterName, Location iterLoc,
                      Node control, bool isReversed);
    Node endForStmt(Node forNode, NodeVector &bodyNodes);

    Node acceptDSTDefinition(Node name, Node lower, Node upper);
    Node acceptDSTDefinition(Node nameOrAttribute, bool isUnconstrained);
    Node acceptDSTDefinition(Node lower, Node upper);

    bool acceptObjectDeclaration(Location loc, IdentifierInfo *name,
                                 Node type, Node initializer);

    bool acceptRenamedObjectDeclaration(Location loc, IdentifierInfo *name,
                                        Node type, Node target);

    void acceptDeclarationInitializer(Node declNode, Node initializer);

    Node acceptPercent(Location loc);

    bool acceptImportDeclaration(Node importedType);

    Node acceptProcedureCall(Node name);

    Node acceptInj(Location loc, Node expr);

    Node acceptPrj(Location loc, Node expr);

    Node acceptIntegerLiteral(llvm::APInt &value, Location loc);

    Node acceptStringLiteral(const char *string, unsigned len, Location loc);

    Node acceptNullExpr(Location loc);

    Node acceptAllocatorExpr(Node operand, Location loc);

    Node acceptQualifiedExpr(Node qualifier, Node operand);

    Node acceptDereference(Node prefix, Location loc);

    Node acceptIfStmt(Location loc, Node condition, NodeVector &consequents);

    Node acceptElseStmt(Location loc, Node ifNode, NodeVector &alternates);

    Node acceptElsifStmt(Location loc, Node ifNode, Node condition,
                         NodeVector &consequents);

    Node acceptEmptyReturnStmt(Location loc);

    Node acceptReturnStmt(Location loc, Node retNode);

    Node acceptAssignmentStmt(Node target, Node value);

    Node beginBlockStmt(Location loc, IdentifierInfo *label = 0);
    void endBlockStmt(Node block);

    Node beginHandlerStmt(Location loc, NodeVector &choices);
    void endHandlerStmt(Node context, Node handler);

    Node acceptNullStmt(Location loc);

    bool acceptStmt(Node context, Node stmt);

    Node acceptWhileStmt(Location loc, Node condition, NodeVector &stmtNodes);

    Node acceptLoopStmt(Location loc, NodeVector &stmtNodes);

    Node acceptRaiseStmt(Location loc, Node exception, Node message);

    Node acceptPragmaStmt(IdentifierInfo *name, Location loc, NodeVector &args);

    void acceptPragmaImport(Location pragmaLoc,
                            IdentifierInfo *convention, Location conventionLoc,
                            IdentifierInfo *entity, Location entityLoc,
                            Node externalNameNode);

    void beginEnumeration(IdentifierInfo *name, Location loc);
    void acceptEnumerationIdentifier(IdentifierInfo *name, Location loc);
    void acceptEnumerationCharacter(IdentifierInfo *name, Location loc);
    void endEnumeration();

    void acceptIntegerTypeDecl(IdentifierInfo *name, Location loc,
                               Node low, Node high);

    void acceptRangedSubtypeDecl(IdentifierInfo *name, Location loc,
                                 Node subtype, Node low, Node high);

    void acceptSubtypeDecl(IdentifierInfo *name, Location loc, Node subtype);
    void acceptIncompleteTypeDecl(IdentifierInfo *name, Location loc);
    void acceptAccessTypeDecl(IdentifierInfo *name, Location loc, Node subtype);

    void acceptArrayDecl(IdentifierInfo *name, Location loc,
                         NodeVector indices, Node component);

    void beginRecord(IdentifierInfo *name, Location loc);
    void acceptRecordComponent(IdentifierInfo *name, Location loc, Node type);
    void endRecord();

    // Delete the underlying Ast node.
    void deleteNode(Node &node);
    //@}

    /// \name Generic Accessors and Predicates.
    ///
    //@{

    /// \brief Returns true if the type checker has not encountered an error and
    /// false otherwise.
    bool checkSuccessful() const { return diagnostic.numErrors() == 0; }

    /// Returns the compilation which this type checker populates with well
    /// formed top-level nodes.
    CompilationUnit *getCompilationUnit() const { return compUnit; }

    /// Returns the Diagnostic object thru which diagnostics are posted.
    Diagnostic &getDiagnostic() { return diagnostic; }

    /// Returns the AstResource used by the type checker to construct AST nodes.
    AstResource &getAstResource() { return resource; }
    //@}

    /// \name General Semantic Analysis.
    //@{

    /// Returns true if the type \p source requires a conversion to be
    /// compatible with the type \p target.
    static bool conversionRequired(Type *source, Type *target);

    /// Wraps the given expression in a ConversionExpr if needed.
    static Expr *convertIfNeeded(Expr *expr, Type *target);

    /// Returns a dereferenced type of \p source which covers the type \p target
    /// or null if no such type exists.
    Type *getCoveringDereference(Type *source, Type *target);

    /// Returns a dereferenced type of \p source which satisfies the given
    /// target classification or null if no such type exists.
    Type *getCoveringDereference(Type *source, Type::Classification ID);

    /// Implicitly wraps the given expression in DereferenceExpr nodes utill its
    /// type covers \p target.
    ///
    /// The given expression must have an access type which can be dereferenced
    /// to the given target type.
    Expr *implicitlyDereference(Expr *expr, Type *target);

    /// Implicitly wraps the given expression in DereferenceExpr nodes utill its
    /// type satisfies the given type classification.
    ///
    /// The given expression must have an access type which can be dereferenced
    /// to yield a type beloging to the given classification.
    Expr *implicitlyDereference(Expr *expr, Type::Classification ID);

    /// \brief Typechecks the given expression in the given type context.
    ///
    /// This is a main entry point into the top-down phase of the type checker.
    /// This method returns a potentially different expression from its argument
    /// on success and null on failure.
    Expr *checkExprInContext(Expr *expr, Type *context);

    /// Typechecks the given expression using the given type classification as
    /// context.  This method returns true if the expression was successfully
    /// checked.  Otherwise, false is returned an diagnostics are emitted.
    bool checkExprInContext(Expr *expr, Type::Classification ID);

    /// Typechecks the given \em resolved expression in the given type context
    /// introducing any implicit dereferences required to conform to the context
    /// type.
    ///
    /// Returns the (possibly updated) expression on success or null on error.
    Expr *checkExprAndDereferenceInContext(Expr *expr, Type *context);

    /// Returns true if \p expr is a static integer expression.  If so,
    /// initializes \p result to a signed value which can accommodate the given
    /// static expression.
    bool ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result);

    /// Returns true if \p expr is a static integer expression.  Otherwise false
    /// is returned and diagnostics are posted.
    bool ensureStaticIntegerExpr(Expr *expr);

    /// Checks if \p node resolves to an expression and returns that expression
    /// on success.  Else null is returned and diagnostics are posted.
    Expr *ensureExpr(Node node);

    /// Checks if \p node resolves to an expression and returns that expression
    /// on success.  Else null is returned and diagnostics are posted.
    Expr *ensureExpr(Ast *node);

    /// Checks that the given identifier is a valid direct name.  Returns an Ast
    /// node representing the name if the checks succeed and null otherwise.
    ///
    /// \param name The identifier to check.
    ///
    /// \param loc The location of \p name.
    ///
    /// \param forStatement If true, check that \p name denotes a procedure.
    Ast *checkDirectName(IdentifierInfo *name, Location loc, bool forStatement);

    /// Basic type equality predicate.
    static bool covers(Type *A, Type *B);
    //@}

private:
    Diagnostic      &diagnostic;
    AstResource     &resource;
    CompilationUnit *compUnit;
    DeclRegion      *declarativeRegion;
    ModelDecl       *currentModel;
    Scope            scope;

    /// The set of AbstractDomainDecls serving as parameters to the current
    /// capsule.
    llvm::SmallVector<AbstractDomainDecl *, 8> GenericFormalDecls;

    /// Stencil classes used to hold intermediate results.
    EnumDeclStencil enumStencil;
    SRDeclStencil routineStencil;

    /// Aggregates can nest.  The following stack is used to maintain the
    /// current context when processing aggregate expressions.
    std::stack<AggregateExpr*> aggregateStack;

    /// Several support routines operate over llvm::SmallVector's.  Define a
    /// generic shorthand.
    template <class T>
    struct SVImpl {
        typedef llvm::SmallVectorImpl<T> Type;
    };

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

    // A function object version of lift_node.
    template <class T>
    struct NodeLifter : public std::unary_function<Node&, T*> {
        T* operator ()(Node &node) const { return lift_node<T>(node); }
    };

    // A function object version of cast_node.
    template <class T>
    struct NodeCaster : public std::unary_function<Node&, T*> {
        T* operator ()(Node &node) const { return cast_node<T>(node); }
    };

    DeclRegion *currentDeclarativeRegion() const {
        return declarativeRegion;
    }

    void pushDeclarativeRegion(DeclRegion *region) {
        declarativeRegion = region;
    }

    DeclRegion *popDeclarativeRegion() {
        DeclRegion *res = declarativeRegion;
        declarativeRegion = res->getParent();
        return res;
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

    // Returns the type of % for the current model, or 0 if we are not currently
    // processing a model.
    DomainType *getCurrentPercentType() const;

    // Returns the % node for the current model, or 0 if we are not currently
    // processing a model.
    PercentDecl *getCurrentPercent() const;

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
    // scope with the default environment specified by Comma (declarations of
    // primitive types like Boolean, for example).
    void populateInitialEnvironment();

    /// Helper to beginDomainDecl and beginSignatureDecl.
    ///
    /// Assumes the current model has been set.  Performs the common actions
    /// necessary to begin processing both signature and domain declarations.
    void initializeForModelDeclaration();

    /// Returns true if the subroutines \p X and \p Y are compatible.
    ///
    /// This is a stronger test than just type equality.  Two subroutine
    /// declarations are compatible if:
    ///
    ///    - They both have the same name,
    ///
    ///    - they both have the same type.
    ///
    ///    - they both have the same parameter mode profile, and,
    ///
    ///    - they both have the same keywords.
    bool compatibleSubroutineDecls(SubroutineDecl *X, SubroutineDecl *Y);

    /// If the given functor represents the current capsule being checked,
    /// ensure that none of the argument types directly reference %.  Returns
    /// true if the given functor and argument combination is legal, otherwise
    /// false is returned and diagnostics are posted.
    bool ensureNonRecursiveInstance(FunctorDecl *decl,
                                    DomainTypeDecl **args, unsigned numArgs,
                                    Location loc);

    /// Brings the implicit declarations provided by \p decl into scope.
    void acquireImplicitDeclarations(Decl *decl);

    /// Using the current model as context, rewrites the set of immediate
    /// declarations provided by the given signature.  Each declaration that
    /// does not conflict with another declaration in scope is is added both to
    /// the scope and to the current DeclRegion.  The given Location is the
    /// position of the super signature indication.  This method is used to
    /// implement acceptSupersignature.
    void acquireImmediateSignatureDeclarations(SigInstanceDecl *sig,
                                               Location loc);

    /// Ensures the given Node resolves to a complete type declaration.
    ///
    /// \see ensureCompleteTypeDecl(Decl, Location, bool);
    TypeDecl *ensureCompleteTypeDecl(Node refNode, bool report = true);

    /// Ensures that the given declaration node denotes a complete type
    /// declaration.
    ///
    /// \param decl The declaration node to check.
    ///
    /// \param loc The location to use for any diagnostic messages.
    ///
    /// \param report When true and the type could not be resolved to a complete
    /// type declaration diagnostics are posted.
    ///
    /// \return The complete type declaration if resolvable else null.
    TypeDecl *ensureCompleteTypeDecl(Decl *decl, Location loc,
                                     bool report = true);

    /// Ensures that the given declaration node denotes a type declaration.
    ///
    /// This method is less strict than ensureCompleteTypeDecl as it does note
    /// check that the given declaration denotes a complete type.
    TypeDecl *ensureTypeDecl(Decl *decl, Location loc, bool report = true);

    /// Ensures the given Node resolves to a type declaration.
    ///
    /// \see ensumeTypeDecl(Decl, Location, bool);
    TypeDecl *ensureTypeDecl(Node refNode, bool report = true);

    /// Similar to ensureCompleteTypeDecl, but operates over type nodes.
    Type *resolveType(Type *type) const;

    /// Resolves the type of the given expression.
    Type *resolveType(Expr *expr) const { return resolveType(expr->getType()); }

    // Resolves the type of the given integer literal, and ensures that the
    // given type context is itself compatible with the literal provided.
    // Returns a valid expression node (possibly different from \p intLit) if
    // the literal was successfully checked.  Otherwise, null is returned and
    // appropriate diagnostics are posted.
    Expr *resolveIntegerLiteral(IntegerLiteral *intLit, Type *context);

    // Resolved the type of the given integer literal with respect to the given
    // type classification.  Returns true if the literal was successfully
    // checked.  Otherwise false is returned and appropriate diagnostics are
    // posted.
    bool resolveIntegerLiteral(IntegerLiteral *intLit, Type::Classification ID);

    // Resolves the type of the given string literal, and ensures that the given
    // type context is itself compatible with the literal provided.  Returns a
    // valid expression if the literal was successfully checked (possibly
    // different from \p strLit).  Otherwise, null is returned and appropriate
    // diagnostics are posted.
    Expr *resolveStringLiteral(StringLiteral *strLit, Type *context);

    // Resolves the type of the an aggregate expression with respect to the
    // given type context.  Returns a valid expression if the aggregate was
    // successfully checked (possibly different from \p agg).  Otherwise, null
    // is returned and appropriate diagnostics are posted.
    Expr *resolveAggregateExpr(AggregateExpr *agg, Type *context);

    // Resolves a null expression if possible given a target type.  Returns a
    // valid expression node if \p expr was found to be compatible with \p
    // context.  Otherwise null is returned and diagnostics are posted.
    Expr *resolveNullExpr(NullExpr *expr, Type *context);

    // Resolves an allocator expression with respect to the given target type.
    // Returns a valid expression node on success and null otherwise.
    Expr *resolveAllocatorExpr(AllocatorExpr *alloc, Type *context);

    // Resolves a selected component expression with respect to the given target
    // type.
    Expr *resolveSelectedExpr(SelectedExpr *select, Type *context);

    // Resolves a diamond expression to the given target type.
    Expr *resolveDiamondExpr(DiamondExpr *diamond, Type *context);

    // Resolves the given call expression to one which satisfies the given
    // target type.  Returns a valid expression if the call was successfully
    // checked (possibly different from \p call).  Otherwise, null is returned
    // and the appropriate diagnostics are emitted.
    Expr *resolveFunctionCall(FunctionCallExpr *call, Type *type);

    // Resolves the given call expression to one which satisfies the given type
    // classification and returns true.  Otherwise, false is returned and the
    // appropriate diagnostics are emitted.
    bool resolveFunctionCall(FunctionCallExpr *call, Type::Classification ID);

    // Resolves the given call expression to one which returns a record type
    // that in turn provides a component with the given name and type.  Returns
    // a valid expression if the call was successfully resolved (possibly
    // different from \p call).  Otherwise, null is returned and diagnostics are
    // posted.
    Expr *resolveFunctionCall(FunctionCallExpr *call, IdentifierInfo *selector,
                              Type *targetType);

    // Returns the prefered connective for the given ambiguous function call, or
    // null if no unambiguous interpretation exists.
    FunctionDecl *resolvePreferredConnective(FunctionCallExpr *Call,
                                             Type *targetType);

    // Checks if the given set of function declarations contains a preferred
    // primitive operator which should be preferred over any other.  Returns the
    // prefered declaration if found and null otherwise.
    FunctionDecl *resolvePreferredOperator(SVImpl<FunctionDecl*>::Type &decls);

    /// Checks that the given SubroutineRef can be applied to the given argument
    /// nodes.
    ///
    /// \return An SubroutineCall node representing the result of the
    /// application, an IndexedArrayExpr, or null if the call did not type
    /// check.
    Ast *acceptSubroutineApplication(SubroutineRef *ref,
                                     NodeVector &argNodes);

    /// Given a vector \p argNodes of Node's representing the arguments to a
    /// subroutine call, extracts the AST nodes and fills in the vectors \p
    /// positional and \p keyed with the positional and keyed arguments,
    /// respectively.
    ///
    /// This method ensures that the argument nodes are generally compatible
    /// with a subroutine call; namely, that all positional and keyed arguments
    /// denote Expr's.  If this conversion fails, diagnostics are posted, false
    /// is returned, and the contents of the given vectors is undefined.
    bool checkSubroutineArgumentNodes(NodeVector &argNodes,
                                      SVImpl<Expr*>::Type &positional,
                                      SVImpl<KeywordSelector*>::Type &keyed);

    /// Checks that the given expression \p arg satisifes the type \p
    /// targetType.  Also ensures that \p arg is compatible with the given
    /// parameter mode.  Returns a possibly updated expression node if the check
    /// succeed and null otherwise.
    Expr *checkSubroutineArgument(Expr *arg, Type *targetType,
                                  PM::ParameterMode targetMode);

    /// Applys checkSubroutineArgument() to each argument of the given call node
    /// (which must be resolved).
    bool checkSubroutineCallArguments(SubroutineCall *call);

    /// Checks that given arguments are compatible with those of the given
    /// decl.
    ///
    /// It is assumed that the number of arguments passed matches the number
    /// expected by the decl.  Each argument is checked for type and mode
    /// compatibility.  Also, each keyed argument is checked to ensure that the
    /// key exists, that the argument does not conflict with a positional
    /// parameter, and that all keys are unique.  Returns true if the check
    /// succeeds, false otherwise and appropriate diagnostics are posted.
    bool checkSubroutineArguments(SubroutineDecl *decl,
                                  SVImpl<Expr*>::Type &posArgs,
                                  SVImpl<KeywordSelector*>::Type &keyArgs);

    /// Checks that the given subroutine decl accepts the provided positional
    /// arguments.
    bool routineAcceptsArgs(SubroutineDecl *decl,
                            SVImpl<Expr*>::Type &args);

    /// Checks that the given subroutine decl accepts the provided keyword
    /// arguments.
    bool routineAcceptsArgs(SubroutineDecl *decl,
                            SVImpl<KeywordSelector*>::Type &args);

    /// Checks a possibly ambiguous (overloaded) SubroutineRef \p ref given a
    /// set of positional and keyed arguments.
    ///
    /// \return A general AST node which is either a FunctionCallExpr,
    /// ProcedureCallStmt, ArrayIndexExpr, or null if the call did not type
    /// check.
    Ast *acceptSubroutineCall(SubroutineRef *ref,
                              SVImpl<Expr*>::Type &positionalArgs,
                              SVImpl<KeywordSelector*>::Type &keyedArgs);

    /// Given a resolved (not overloaded) SubroutineRef \p ref, and a set of
    /// positional and keyed arguments, checks the arguments and builds an AST
    /// node representing the call.
    ///
    /// \return A general AST node which is either a FunctionCallExpr,
    /// ProcedureCallStmt, or null if the call did not type check.
    SubroutineCall *
    checkSubroutineCall(SubroutineRef *ref,
                        SVImpl<Expr*>::Type &positionalArgs,
                        SVImpl<KeywordSelector*>::Type &keyArgs);

    /// Injects implicit ConversionExpr nodes into the positional

    /// Returns true if \p expr is compatible with the given type.
    ///
    /// This routine does not post diagnostics.  It is used to do a speculative
    /// check of a subroutine argument when resolving a set of overloaded
    /// declarations.
    bool checkApplicableArgument(Expr *expr, Type *targetType);

    // Returns true if the given type is compatible with the given abstract
    // domain decl in the environment established by the given rewrites.
    //
    // In this case, the source must denote a domain which satisfies the entire
    // signature profile of the target.  The supplied location indicates the
    // position of the source type.
    bool checkSignatureProfile(const AstRewriter &rewrites, Type *source,
                               AbstractDomainDecl *target, Location loc);

    // Verifies that the given AddDecl satisfies the constraints imposed by its
    // signature.  Returns true if the constraints are satisfied.  Otherwise,
    // false is returned and diagnostics are posted.
    bool ensureExportConstraints(AddDecl *add);

    // Returns true if the given decl is equivalent to % in the context of the
    // current domain.
    bool denotesDomainPercent(const Decl *decl);

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
                               DomainTypeDecl **args, unsigned numArgs);

    /// Resolves the argument type of a Functor or Variety given previous actual
    /// arguments.
    ///
    /// For a dependent argument list of the form <tt>(X : T, Y : U(X))</tt>,
    /// this function resolves the type of \c U(X) given an actual parameter for
    /// \c X.  It is assumed that the actual arguments provided are compatible
    /// with the given model.
    SigInstanceDecl *resolveFormalSignature(ModelDecl *parameterizedModel,
                                            Type **arguments,
                                            unsigned numArguments);

    /// Returns true if the given parameter is of mode "in", and thus capatable
    /// with a function declaration.  Otherwise false is returned an a
    /// diagnostic is posted.
    bool checkFunctionParameter(ParamValueDecl *param);

    /// Helper for acceptEnumerationIdentifier and acceptEnumerationCharacter.
    /// Forms a generic enumeration literal AST node.  Returns true if the
    /// literal was generated successfully.
    bool acceptEnumerationLiteral(IdentifierInfo *name, Location loc);

    /// Adds the declarations present in the given region to the current scope
    /// as direct names.  This subroutine is used to introduce the implicit
    /// operations which accompany a type declaration.  If the region introduces
    /// any conflicting names a diagnostic is posted and the corresponding
    /// declaration is not added.
    void introduceImplicitDecls(DeclRegion *region);

    /// Utility routine for building TypeRef nodes over the given model.
    TypeRef *buildTypeRefForModel(Location loc, ModelDecl *mdecl);

    /// Processes the indirect names in the given resolver.  If no indirect
    /// names could be found, a diagnostic is posted and and null is returned.
    Ast *checkIndirectName(Location loc, Resolver &resolver);

    /// Checks that the given TypeRef can be applied to the given arguments.
    ///
    /// \return A new TypeRef representing the application if successful and
    /// null otherwise.
    TypeRef *acceptTypeApplication(TypeRef *ref, NodeVector &argNodes);

    /// Checks that the given TypeRef can be applied to the given arguments.
    ///
    /// \return A new TypeRef representing the application if successful and
    /// null otherwise.
    TypeRef *acceptTypeApplication(TypeRef *ref,
                                   SVImpl<TypeRef *>::Type &posArgs,
                                   SVImpl<KeywordSelector *>::Type &keyedArgs);

    /// Given a vector \p argNodes of Node's representing the arguments to a
    /// subroutine call, extracts the AST nodes and fills in the vectors \p
    /// positional and \p keyed with the positional and keyed arguments,
    /// respectively.
    ///
    /// This method ensures that the argument nodes are generally compatible
    /// with a type application; namely, that all positional and keyed arguments
    /// denote TypeRef's.  If this conversion fails, diagnostics are posted,
    /// false is returned, and the contents of the given vectors is undefined.
    bool checkTypeArgumentNodes(NodeVector &argNodes,
                                SVImpl<TypeRef*>::Type &positional,
                                SVImpl<KeywordSelector*>::Type &keyed);

    /// Checks that the given TypeRef is valid as a type parameter.  If the
    /// check fails, diagnostics are posted.
    ///
    /// \return The DomainTypeDecl corresponding to \p ref (the only valid kind
    /// of type parameter ATM), or null.
    DomainTypeDecl *ensureValidModelParam(TypeRef *ref);

    /// Checks that the given keyword arguments can be used as arguments to the
    /// given model (which must be parameterized), assuming \p numPositional
    /// positional parameters are a part of the call.  For example, given:
    ///
    /// \verbatim
    ///     F(X, Y, P => Z);
    /// \endverbatim
    ///
    /// Then \p numPositional would be 2 and \p keyedArgs would be a vector of
    /// length 1 containing the keyword selector representing <tt>P => Z</tt>.
    bool checkModelKeywordArgs(ModelDecl *model, unsigned numPositional,
                               SVImpl<KeywordSelector*>::Type &keyedArgs);

    /// Ensures that the given arguments \p args is compatible with the
    /// parameterized mode \p model. The \p argLocs vector contains a Location
    /// entry for each argument.
    ///
    /// This method handles any dependency relationships amongst the models
    /// formal parameters.  Returns true if the checks succeed, otherwise false
    /// is returned and diagnostics are posted.
    bool checkModelArgs(ModelDecl *model,
                        SVImpl<DomainTypeDecl*>::Type &args,
                        SVImpl<Location>::Type &argLocs);

    /// Introduces the given type declaration node into the current scope and
    /// declarative region.
    ///
    /// This method performs several checks.  First, it ensures that the
    /// declaration does not conflict with any other declaration in the current
    /// scope.  Second, if the given declaration can serve as a completion for a
    /// visible incomplete type declaration the necessary updates to the
    /// corresponding incomplete declaration node is performed.
    ///
    /// \return True if the declaration was successfully added into the scope
    /// and current declarative region.  Otherwise false is returned and
    /// diagnostics are posted.
    bool introduceTypeDeclaration(TypeDecl *decl);

    /// Builds an IndexedArrayExpr.
    ///
    /// Checks that \p expr is of array type, and ensures that the given
    /// argument nodes can serve as indexes into \p expr.
    ///
    /// \return An IndexedArrayExpr if the expression is valid and null
    /// otherwise.
    IndexedArrayExpr *acceptIndexedArray(Expr *ref, NodeVector &argNodes);

    /// Checks that the given nodes are generally valid as array indices
    /// (meaning that they must all resolve to denote Expr's).  Fills in the
    /// array \p indices with the results.  If the conversion fails, then false
    /// is returned, diagnostics are posted, and the state of \p indices is
    /// undefined.
    bool checkArrayIndexNodes(NodeVector &argNodes,
                              SVImpl<Expr*>::Type &indices);

    /// Typechecks an indexed array expression.
    ///
    /// \return An IndexedArrayExpr on success or null on failure.
    IndexedArrayExpr *acceptIndexedArray(Expr *ref,
                                         SVImpl<Expr*>::Type &indices);

    /// Checks an object declaration of array type.  This method is a special
    /// case version of acceptObjectDeclaration().
    ///
    /// \param loc The location of the declaration.
    ///
    /// \param name The object identifier.
    ///
    /// \param arrDecl Declaration node corresponding to the type of the object.
    ///
    /// \param init Initialization expression, or null if there is no
    /// initializer.
    ///
    /// \return An ObjectDecl node or null if the declaration did not type
    /// check.
    ObjectDecl *acceptArrayObjectDeclaration(Location loc, IdentifierInfo *name,
                                             ArrayDecl *arrDecl, Expr *init);

    /// Returns a constrained array subtype derived from the given type \p arrTy
    /// and an array valued expression (typically an array initializer) \p init.
    ArrayType *getConstrainedArraySubtype(ArrayType *arrTy, Expr *init);

    /// Creates a FunctionCallExpr or ProcedureCallStmt representing the given
    /// SubroutineRef, provided that \p ref admits declarations with arity zero.
    /// On failure, null is returned and diagnostics posted.
    Ast *finishSubroutineRef(SubroutineRef *ref);

    /// Ensures that the given TypeRef denotes a fully applied type (as opposed
    /// to an incomplete reference to a variety or functor).  Returns true if
    /// the check succeeds.  Otherwise false is returned and diagnostics are
    /// posted.
    bool finishTypeRef(TypeRef *ref);

    /// Helper method for acceptSelectedComponent.  Handles the case when the
    /// prefix denotes a type.
    Ast *processExpandedName(TypeRef *ref, IdentifierInfo *name, Location loc,
                             bool forStatement);

    /// Helper method for acceptSelectedComponent.  Handles the case when the
    /// prefix denotes an expression.
    Ast *processSelectedComponent(Expr *expr, IdentifierInfo *name,
                                  Location loc, bool forStatement);

    /// Checks an assert pragma with the given arguments.
    PragmaAssert *acceptPragmaAssert(Location loc, NodeVector &args);

    Ast *checkAttribute(attrib::AttributeID, Ast *prefix, Location loc);

    /// Returns the location of \p node.
    static Location getNodeLoc(Node node);

    bool has(DomainType *source, SigInstanceDecl *target);
    bool has(const AstRewriter &rewrites,
             DomainType *source, AbstractDomainDecl *target);

    SourceLocation getSourceLoc(Location loc) const {
        return resource.getTextProvider().getSourceLocation(loc);
    }

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        return diagnostic.report(getSourceLoc(loc), kind);
    }
};

} // End comma namespace

#endif
