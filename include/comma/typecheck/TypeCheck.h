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
#include "comma/ast/Cunit.h"
#include "comma/ast/AstResource.h"
#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include "comma/parser/ParseClient.h"

#include "llvm/Support/Casting.h"

#include <iosfwd>

namespace llvm {

class APInt;

} // end llvm namespace.

namespace comma {

class DeclProducer;
class Scope;

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

    Node acceptNestedQualifier(Node qualifier, Node qualifierType,
                               Location loc);

    Node acceptIntegerLiteral(llvm::APInt &value, Location loc);

    Node acceptIfStmt(Location loc, Node condition, NodeVector &consequents);

    Node acceptElseStmt(Location loc, Node ifNode, NodeVector &alternates);

    Node acceptElsifStmt(Location loc, Node ifNode, Node condition,
                         NodeVector &consequents);

    Node acceptEmptyReturnStmt(Location loc);

    Node acceptReturnStmt(Location loc, Node retNode);

    Node acceptAssignmentStmt(Location loc,
                              IdentifierInfo *target, Node value);

    // Called when a block statement is about to be parsed.
    Node beginBlockStmt(Location loc, IdentifierInfo *label = 0);

    // This method is called for each statement associated with the block.
    void acceptBlockStmt(Node block, Node stmt);

    // Once the last statement of a block has been parsed, this method is called
    // to inform the client that we are leaving the block context established by
    // the last call to beginBlockStmt.
    void endBlockStmt(Node block);

    Node acceptWhileStmt(Location loc, Node condition, NodeVector &stmtNodes);

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
    Scope           *scope;
    unsigned         errorCount;

    /// Instance of a helper class used to hold primitive types and to construct
    /// implicit operations.
    DeclProducer *declProducer;

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

    void ensureNecessaryRedeclarations(ModelDecl *model);

    /// If the given functor represents the current capsule being checked,
    /// ensure that none of the argument types directly reference %.  Returns
    /// true if the given functor and argument combination is legal, otherwise
    /// false is returned and diagnostics are posted.
    bool ensureNonRecursiveInstance(FunctorDecl *decl,
                                    Type **args, unsigned numArgs,
                                    Location loc);

    void aquireSignatureTypeDeclarations(ModelDecl *model, Sigoid *sigdecl);

    void aquireSignatureImplicitDeclarations(ModelDecl *model, Sigoid *sigdecl);

    DomainType *ensureDomainType(Node typeNode, Location loc, bool report = true);
    DomainType *ensureDomainType(Type *type, Location loc, bool report = true);
    Type *ensureValueType(Node typeNode, Location loc, bool report = true);
    Type *ensureValueType(Type *type, Location loc, bool report = true);

    /// Returns true if \p expr is a static integer expression.  If so,
    /// initializes \p result to a signed value which can accommodate the given
    /// static expression.
    bool ensureStaticIntegerExpr(Expr *expr, llvm::APInt &result);

    Node acceptQualifiedName(Node            qualNode,
                             IdentifierInfo *name,
                             Location        loc);

    Expr *resolveDirectDecl(IdentifierInfo *name, Location loc);

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

    // Resolves the given call expression to one which satisfies the given
    // target type and returns true.  Otherwise, false is returned and the
    // appropriate diagnostics are emitted.
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

    /// Checks that the supplied array of arguments are mode compatible with
    /// those of the given decl.  This is a helper method for
    /// checkSubroutineCall.
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

    // Returns true if the source type is compatible with the target.  In this
    // case the target denotes a signature, and so the source must be a domain
    // which satisfies the signature constraint.  The supplied location
    // indicates the position of the source type.
    bool checkType(Type *source, SigInstanceDecl *target, Location loc);

    // Verifies that the given AddDecl satisfies the constraints imposed by its
    // signature.  Returns true if the constraints are satisfied.  Otherwise,
    // false is returned and diagnostics are posted.
    bool ensureExportConstraints(AddDecl *add);

    /// Returns true if the IdentifierInfo \p info can name a binary function.
    bool namesBinaryFunction(IdentifierInfo *info);

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
                               Type **args, unsigned numArgs);

    /// Resolves the argument type of a Functor or Variety given previous actual
    /// arguments.
    ///
    /// For a dependent argument list of the form <tt>(X : T, Y : U(X))</tt>,
    /// this function resolves the type of \c U(X) given an actual parameter for
    /// \c X.  It is assumed that the actual arguments provided are compatable
    /// with the given model.
    SigInstanceDecl *
    resolveFormalSignature(ModelDecl *parameterizedModel,
                           Type **arguments, unsigned numArguments);

    /// Returns true if the given parameter is of mode "in", and thus capatable
    /// with a function declaration.  Otherwise false is returned an a
    /// diagnostic is posted.
    bool checkFunctionParameter(ParamValueDecl *param);

    /// Returns true if the given descriptor does not contain any duplicate
    /// formal parameters.  Otherwise false is returned and the appropriate
    /// diagnostic is posted.
    bool checkDescriptorDuplicateParams(Descriptor &desc);

    /// Returns true if the descriptor does not contain any parameters with the
    /// given name.  Otherwise false is returned and the appropriate diagnostic
    /// is posted.
    bool checkDescriptorDuplicateParams(Descriptor &desc,
                                        IdentifierInfo *idInfo, Location loc);

    /// Given a container type \p V with a push_back method, this function
    /// converts the parameters of the descriptor to type T and appends them to
    /// \p vec.
    template <class T, class V>
    void convertDescriptorParams(Descriptor &desc, V &vec) {
        typedef Descriptor::paramIterator iterator;
        iterator I = desc.beginParams();
        iterator E = desc.endParams();
        for ( ; I != E; ++I)
            vec.push_back(cast_node<T>(*I));
    }

    /// Imports the given declarative region into the current scope.
    void importDeclRegion(DeclRegion *region);

    bool has(DomainType *source, SigInstanceDecl *target);

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
