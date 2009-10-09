//===-- typecheck/TypeCheck.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_TYPECHECK_HDR_GUARD
#define COMMA_TYPECHECK_TYPECHECK_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Cunit.h"
#include "comma/ast/Type.h"
#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include "comma/parser/ParseClient.h"

#include "llvm/Support/Casting.h"

#include <iosfwd>

namespace llvm {

class APInt;

} // end llvm namespace.

namespace comma {

class Resolver;
class Scope;

class ArrayDeclStencil;
class EnumDeclStencil;
class SRDeclStencil;

class TypeCheck : public ParseClient {

public:
    TypeCheck(Diagnostic      &diag,
              AstResource     &resource,
              CompilationUnit *cunit);

    ~TypeCheck();

    void beginCapsule();
    void endCapsule();

    void beginGenericFormals();
    void endGenericFormals();

    void beginFormalDomainDecl(IdentifierInfo *name, Location loc);
    void endFormalDomainDecl();

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

    void acceptOverrideTarget(Node prefix, IdentifierInfo *target, Location loc);

    Node endSubroutineDeclaration(bool definitionFollows);


    void beginSubroutineDefinition(Node declarationNode);

    void acceptSubroutineStmt(Node stmt);

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

    bool acceptObjectDeclaration(Location loc, IdentifierInfo *name,
                                 Node type, Node initializer);


    void acceptDeclarationInitializer(Node declNode, Node initializer);

    Node acceptPercent(Location loc);

    bool acceptImportDeclaration(Node importedType);

    Node acceptProcedureCall(Node name);

    // Called for "inj" expressions.  loc is the location of the inj token and
    // expr is its argument.
    Node acceptInj(Location loc, Node expr);

    // Called for "prj" expressions.  loc is the location of the prj token and
    // expr is its argument.
    Node acceptPrj(Location loc, Node expr);

    Node acceptQualifier(Node qualifierType);

    Node acceptNestedQualifier(Node qualifier, Node qualifierType);

    Node acceptIntegerLiteral(llvm::APInt &value, Location loc);

    Node acceptStringLiteral(const char *string, unsigned len, Location loc);

    Node acceptIfStmt(Location loc, Node condition, NodeVector &consequents);

    Node acceptElseStmt(Location loc, Node ifNode, NodeVector &alternates);

    Node acceptElsifStmt(Location loc, Node ifNode, Node condition,
                         NodeVector &consequents);

    Node acceptEmptyReturnStmt(Location loc);

    Node acceptReturnStmt(Location loc, Node retNode);

    Node acceptAssignmentStmt(Node target, Node value);

    // Called when a block statement is about to be parsed.
    Node beginBlockStmt(Location loc, IdentifierInfo *label = 0);

    // This method is called for each statement associated with the block.
    void acceptBlockStmt(Node block, Node stmt);

    // Once the last statement of a block has been parsed, this method is called
    // to inform the client that we are leaving the block context established by
    // the last call to beginBlockStmt.
    void endBlockStmt(Node block);

    Node acceptWhileStmt(Location loc, Node condition, NodeVector &stmtNodes);

    Node acceptPragmaStmt(IdentifierInfo *name, Location loc, NodeVector &args);

    void beginEnumeration(IdentifierInfo *name, Location loc);
    void acceptEnumerationIdentifier(IdentifierInfo *name, Location loc);
    void acceptEnumerationCharacter(IdentifierInfo *name, Location loc);
    void endEnumeration();

    /// Called to process integer type definitions.
    ///
    /// For example, given a definition of the form <tt>type T is range
    /// X..Y;</tt>, this callback is invoked with \p name set to the identifier
    /// \c T, \p loc set to the location of \p name, \p low set to the
    /// expression \c X, and \p high set to the expression \c Y.
    void acceptIntegerTypedef(IdentifierInfo *name, Location loc,
                              Node low, Node high);

    void beginArray(IdentifierInfo *name, Location loc);
    void acceptUnconstrainedArrayIndex(Node indexNode);
    void acceptArrayIndex(Node indexNode);
    void acceptArrayComponent(Node componentNode);
    void endArray();

    // Delete the underlying Ast node.
    void deleteNode(Node &node);

    /// \brief Returns true if the type checker has not encountered an error and
    /// false otherwise.
    bool checkSuccessful() const { return !diagnostic.reportsGenerated(); }

    /// Returns true if the type \p source requires a conversion to be
    /// compatable with the type \p target.
    static bool conversionRequired(Type *source, Type *target);

private:
    Diagnostic      &diagnostic;
    AstResource     &resource;
    CompilationUnit *compUnit;
    DeclRegion      *declarativeRegion;
    ModelDecl       *currentModel;
    Scope           *scope;

    /// The set of AbstractDomainDecls serving as parameters to the current
    /// capsule.
    llvm::SmallVector<AbstractDomainDecl *, 8> GenericFormalDecls;

    /// Stencil classes used to hold intermediate results.
    ArrayDeclStencil *arrayStencil;
    EnumDeclStencil *enumStencil;
    SRDeclStencil *routineStencil;

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
    // primitive types like Bool, for example).
    void populateInitialEnvironment();

    // Creates a new procedure or function declaration.
    ///
    // Given a subroutine decl, rewrite its type using the supplied rewrites and
    // create a new declaration within the given region.
    static SubroutineDecl *
    makeSubroutineDecl(SubroutineDecl *SRDecl, const AstRewriter &rewrites,
                       DeclRegion *region);

    /// Helper to beginDomainDecl and beginSignatureDecl.
    ///
    /// Assumes the current model has been set.  Performs the common actions
    /// necessary to begin processing both signature and domain declarations.
    void initializeForModelDeclaration();

    void ensureNecessaryRedeclarations(DomainTypeDecl *model);

    /// Returns true if the subroutines \p X and \p Y are compatable.
    ///
    /// This is a stronger test than just type equality.  Two subroutine
    /// declarations are compatable if:
    ///
    ///    - They both have the same name,
    ///
    ///    - they both have the same type.
    ///
    ///    - they both have the same parameter mode profile, and,
    ///
    ///    - they both have the same keywords.
    bool compatibleSubroutineDecls(SubroutineDecl *X, SubroutineDecl *Y);

    /// Checks that subroutines \p X and \p Y have identical parameter mode
    /// profiles, or that an overriding declaration exists in the given region.
    ///
    /// Returns true if the modes match or an override was found, otherwise
    /// false is returned and diagnostics are posted.
    bool ensureMatchingParameterModes(SubroutineDecl *X, SubroutineDecl *Y,
                                      DeclRegion *region);

    /// Checks that the subroutines \p X and \p Y have identical parameter mode
    /// profiles.
    ///
    /// Returns true if the modes match, otherwise false is returned and
    /// diagnostics are posted.
    bool ensureMatchingParameterModes(SubroutineDecl *X, SubroutineDecl *Y);

    /// If the given functor represents the current capsule being checked,
    /// ensure that none of the argument types directly reference %.  Returns
    /// true if the given functor and argument combination is legal, otherwise
    /// false is returned and diagnostics are posted.
    bool ensureNonRecursiveInstance(FunctorDecl *decl,
                                    DomainTypeDecl **args, unsigned numArgs,
                                    Location loc);

    /// Bring all type declarations provided by the signature into the given
    /// declarative region, and register them with the current scope.  If by
    /// bringing any such types into scope results in a name conflict, post a
    /// diagnostic and skip the type.
    void aquireSignatureTypeDeclarations(DeclRegion *region, Sigoid *sigdecl);

    /// Brings the implicit declarations provided by all types supplied by the
    /// given signature into scope.
    void aquireSignatureImplicitDeclarations(Sigoid *sigdecl);

    TypeDecl *ensureTypeDecl(Node refNode, bool report = true);
    TypeDecl *ensureTypeDecl(Decl *decl, Location loc, bool report = true);

    /// Returns true if \p expr is a static integer expression.  If so,
    /// initializes \p result to a signed value which can accommodate the given
    /// static expression.
    bool ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result);

    /// Returns true if \p expr is a static integer expression.  Otherwise false
    /// is returned and diagnostics are posted.
    bool ensureStaticIntegerExpr(Expr *expr);

    /// Resolves a visible declarative region associated with a qualifier.
    ///
    /// Qualifiers can name signature components, but such qualifiers are
    /// permitted in certain contexts -- such qualifiers do not denote ordinary
    /// visible names.  Posts a diagnostic when the qualifier names a signature
    /// and returns null.
    DeclRegion *resolveVisibleQualifiedRegion(Qualifier *qual);

    // Returns the TypeDecl or ModelDecl corresponding to the given name.  If
    // the name is not visible, or if the name is ambiguous, this method returns
    // null and posts appropritate diagnostics.
    Decl *resolveTypeOrModelDecl(IdentifierInfo *name,
                                 Location loc, DeclRegion *region = 0);

    // Typechecks the given expression in the given type context.  This method
    // can update the expression (by resolving overloaded function calls, or
    // assigning a type to an integer literal, for example).  Returns true if
    // the expression was successfully checked.  Otherwise, false is returned
    // and appropriate diagnostics are emitted.
    bool checkExprInContext(Expr *expr, Type *context);

    // Resolves the type of the given integer literal, and ensures that the
    // given type context is itself compatible with the literal provided.
    // Returns true if the literal was successfully checked.  Otherwise, false
    // is returned and appropriate diagnostics are posted.
    bool resolveIntegerLiteral(IntegerLiteral *intLit, Type *context);

    // Resolves the type of the given string literal, and ensures that the given
    // type context is itself compatible with the literal provided.  Returns
    // true if the literal was successfully checked.  Otherwise, false is
    // returned and appropriate diagnostics are posted.
    bool resolveStringLiteral(StringLiteral *strLit, Type *context);

    // Resolves the given call expression to one which satisfies the given
    // target type and returns true.  Otherwise, false is returned and the
    // appropriate diagnostics are emitted.
    bool resolveFunctionCall(FunctionCallExpr *call, Type *type);

    // Resolves the given call expression to one which satisfies the given type
    // classification and returns true.  Otherwise, false is returned and the
    // appropriate diagnostics are emitted.
    bool resolveFunctionCall(FunctionCallExpr *call, Type::Classification ID);

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
    /// application.  This is either a FunctionCallExpr, a ProcedureCallExpr, or
    /// null if the call did not type check.
    SubroutineCall *acceptSubroutineApplication(SubroutineRef *ref,
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
    /// targetType.  Also ensures that \p arg is compatable with the given
    /// parameter mode.  Returns true if the checks succeed, other wise false is
    /// returned and diagnostics are posted.
    bool checkSubroutineArgument(Expr *arg, Type *targetType,
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

    /// Given a possibly ambiguous (overloaded) SubroutineRef \p ref, and a set
    /// of positional and keyed arguments, checks the arguments and builds an
    /// AST node representing the call.
    ///
    /// \return A general AST node which is either a FunctionCallExpr,
    /// ProcedureCallStmt, or null if the call did not type check.
    SubroutineCall *
    acceptSubroutineCall(SubroutineRef *ref,
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

    // Returns true if the source type is compatible with the target.  In this
    // case the target denotes a signature, and so the source must be a domain
    // which satisfies the signature constraint.  The supplied location
    // indicates the position of the source type.
    bool checkType(Type *source, SigInstanceDecl *target, Location loc);

    bool covers(Type *A, Type *B);

    bool subsumes(Type *A, Type *B);


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

    /// Returns true if the IdentifierInfo \p info can name a binary function.
    bool namesBinaryFunction(IdentifierInfo *info);

    /// Returns true if the IdentifierInfo \p info can name a unary function.
    bool namesUnaryFunction(IdentifierInfo *info);

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
    /// \c X.  It is assumed that the actual arguments provided are compatable
    /// with the given model.
    SigInstanceDecl *resolveFormalSignature(ModelDecl *parameterizedModel,
                                            Type **arguments,
                                            unsigned numArguments);

    /// Returns true if the given parameter is of mode "in", and thus capatable
    /// with a function declaration.  Otherwise false is returned an a
    /// diagnostic is posted.
    bool checkFunctionParameter(ParamValueDecl *param);

    /// Using the data in srProfileInfo, validates the target of an overriding
    /// declaration.  Returns true if the validation succeeded and updates the
    /// supplied decl to point at its override.  Otherwise false is returned and
    /// diagnostics are posted.
    bool validateOverrideTarget(SubroutineDecl *overridingDecl);

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
    static TypeRef *buildTypeRefForModel(Location loc, ModelDecl *mdecl);

    /// Utility routine for building SubroutineRef nodes using the subroutine
    /// declarations provided by the given Resolver.
    static SubroutineRef *buildSubroutineRef(Location loc, Resolver &resolver);

    /// Processes the indirect names in the given resolver.  If no indirect
    /// names could be found, a diagnostic is posted and an invalid Node is
    /// returned.
    Node acceptIndirectName(Location loc, Resolver &resolver);


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

    /// Builds an IndexedArrayExpr.
    ///
    /// Checks that the given DeclRefExpr \p ref is of array type, and ensures
    /// that the given argument nodes can serve as indexes into \ref.
    ///
    /// \return An IndexedArrayExpr if the expression is valid and null
    /// otherwise.
    IndexedArrayExpr *acceptIndexedArray(DeclRefExpr *ref,
                                         NodeVector &argNodes);

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
    IndexedArrayExpr *acceptIndexedArray(DeclRefExpr *ref,
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
    ArraySubType *getConstrainedArraySubType(ArraySubType *arrTy, Expr *init);

    /// Creates a FunctionCallExpr or ProcedureCallStmt representing the given
    /// SubroutineRef, provided that \p ref admits declarations with arity zero.
    /// On failure, null is returned and diagnostics posted.
    Ast *finishSubroutineRef(SubroutineRef *ref);

    /// Ensures that the given TypeRef denotes a fully applied type (as opposed
    /// to an incomplete reference to a variety or functor).  Returns true if
    /// the check succeeds.  Otherwise false is returned and diagnostics are
    /// posted.
    bool finishTypeRef(TypeRef *ref);

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
