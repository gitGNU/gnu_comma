//===-- ast/Expr.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_EXPR_HDR_GUARD
#define COMMA_AST_EXPR_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Decl.h"
#include "comma/ast/SubroutineRef.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/APInt.h"
#include "llvm/Support/DataTypes.h"

namespace comma {

//===----------------------------------------------------------------------===//
// Expr
//
// This is the root of the AST hierarchy representing expressions.
class Expr : public Ast {

public:
    Expr(AstKind kind, Type *type, Location loc = 0)
        : Ast(kind), type(type), location(loc) {
        assert(this->denotesExpr());
    }

    Expr(AstKind kind, Location loc = 0)
        : Ast(kind), type(0), location(loc) {
        assert(this->denotesExpr());
    }

    // Returns true if this expression has a single known type associated with
    // it.
    bool hasType() const { return type != 0; }

    // Most expressions have a single well-known type.  Others, such as
    // FunctionCallExpr, can have multiple types temporarily associated with
    // pending resolution in the type checker.
    //
    // If this expression has a single well-known type, this method returns it.
    // However, if the type is not know, this method will assert.  One can call
    // Expr::hasType to know if a type is available.
    Type *getType() const {
        assert(hasType() && "Expr does not have an associated type!");
        return type;
    }

    // Sets the type of this expression.  If the supplied type is NULL, then
    // this expression is not associated with a type and Expr::hasType will
    // subsequently return false.
    void setType(Type *type) { this->type = type; }

    Location getLocation() const { return location; }

    static bool classof(const Expr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesExpr();
    }

private:
    Type    *type;
    Location location;
};

//===----------------------------------------------------------------------===//
// DeclRefExpr
//
// Represents references to declarations in the source code.
class DeclRefExpr : public Expr {

public:
    DeclRefExpr(ValueDecl *decl, Location loc)
        : Expr(AST_DeclRefExpr, decl->getType(), loc),
          declaration(decl) { }

    IdentifierInfo *getIdInfo() const { return declaration->getIdInfo(); }
    const char *getString() const { return declaration->getString(); }

    ValueDecl *getDeclaration() { return declaration; }

    void setDeclaration(ValueDecl *decl) { declaration = decl; }

    static bool classof(const DeclRefExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DeclRefExpr;
    }

private:
    ValueDecl *declaration;
};

//===----------------------------------------------------------------------===//
// KeywordSelector
//
// Nodes which represent the selection of a keyword in a subroutine call.
class KeywordSelector : public Expr {

public:
    /// Construct a keyword selection node.
    ///
    /// \param key  The argument keyword to be selected.
    ///
    /// \param loc  The location of \p key.
    ///
    /// \param expr The expression associated with \p key.
    KeywordSelector(IdentifierInfo *key, Location loc, Expr *expr);

    IdentifierInfo *getKeyword() const { return keyword; }
    void setKeyword(IdentifierInfo *key) { keyword = key; }

    Expr *getExpression() { return expression; }
    const Expr *getExpression() const { return expression; }
    void setExpression(Expr *expr) { expression = expr; }

    static bool classof(const KeywordSelector *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_KeywordSelector;
    }

private:
    IdentifierInfo *keyword;
    Expr           *expression;
};

//===----------------------------------------------------------------------===//
// FunctionCallExpr

class FunctionCallExpr : public Expr {

public:
    /// Creates a function call expression using the given SubroutineRef as a
    /// connective.  The resulting FunctionCallExpr takes ownership of \p
    /// connective, and infers its location from same.
    ///
    /// The given SubroutineRef must reference a set of FunctionDecl's.  If more
    /// than one declaration is associated with the ref, then the function call
    /// is said to be ambiguous.
    FunctionCallExpr(SubroutineRef *connective,
                     Expr **arguments, unsigned numArgs);

    /// Creates a function call expression over the given set of function
    /// declarations.
    ///
    /// The given declarations must all have the same name, and otherwise we
    /// compatible with the construction of a SubroutineRef.
    FunctionCallExpr(FunctionDecl **connectives, unsigned numConnectives,
                     Expr **arguments, unsigned numArgs, Location loc);

    /// Creates a function call expression over a single unique function
    /// declaration.  The resulting call expression is unambiguous.
    FunctionCallExpr(FunctionDecl *fdecl,
                     Expr **arguments, unsigned numArgs, Location loc);

    ~FunctionCallExpr();

    /// Sets the qualifier to this call node, describing the source context of
    /// its invocation.
    void setQualifier(Qualifier *qualifier) { this->qualifier = qualifier; }

    /// Returns true if this function call is qualified.
    bool isQualified() const { return qualifier != 0; }

    /// Returns the qualifier associated with this node, or NULL if no qualifier
    /// has been set.
    Qualifier *getQualifier() { return qualifier; }

    /// Returns the qualifier associated with this node, or NULL if no qualifier
    /// has been set.
    const Qualifier *getQualifier() const { return qualifier; }

    //@{
    /// Returns the connective associcated with this call.  The resulting node
    /// is either a FunctionDecl or OverloadedDeclName.
    SubroutineRef *getConnective() { return connective; }
    const SubroutineRef *getConnective() const { return connective; }
    //@}

    /// Resolved the connective for this call.  This method can only be called
    /// when the call is currently ambiguous.
    void resolveConnective(FunctionDecl *connective);

    /// Returns true if this call is ambiguous.
    bool isAmbiguous() const { return connective->isOverloaded(); }

    /// Returns true if this call is unambiguous.
    bool isUnambiguous() const { return !isAmbiguous(); }

    /// Returns true if this call contains a connective with the given type.
    bool containsConnective(FunctionType *fnTy) const {
        return connective->contains(fnTy);
    }

    /// Returns the number of connectives associated with this call.  The result
    /// is always greater than or equal to one.
    unsigned numConnectives() const { return connective->numDeclarations(); }

    //@{
    /// Returns the \p i'th connective associated with this call.  The returned
    /// node is either an EnumLiteral or a regular FunctionDecl.
    const FunctionDecl *getConnective(unsigned i) const {
        return llvm::cast<FunctionDecl>(connective->getDeclaration(i));
    }
    FunctionDecl *getConnective(unsigned i) {
        return llvm::cast<FunctionDecl>(connective->getDeclaration(i));
    }
    //@}


    //@{
    /// Iterators over the set of connectives associcated with this function
    /// call expression.
    typedef SubroutineRef::fun_iterator connective_iterator;
    connective_iterator begin_connectives() {
        return connective->begin_functions();
    }
    connective_iterator end_connectives() {
        return connective->end_functions();
    }

    typedef SubroutineRef::const_fun_iterator const_connective_iterator;
    const_connective_iterator begin_connectives() const {
        return connective->begin_functions();
    }
    const_connective_iterator end_connectives() const {
        return connective->end_functions();
    }
    //@}

    /// Returns the number of arguments supplied to this call expression.
    unsigned getNumArgs() const { return numArgs; }

    //@{
    /// Returns the \p i'th argument of this call.
    Expr *getArg(unsigned i) {
        assert(i < numArgs && "Index out of range!");
        return arguments[i];
    }

    const Expr *getArg(unsigned i) const {
        assert(i < numArgs && "Index out of range!");
        return arguments[i];
    }
    //@}

    /// Sets the \p i'th argument to the given expression.
    void setArg(Expr *expr, unsigned i) {
        assert(i < numArgs && "Index out of range!");
        arguments[i] = expr;
    }

    // Support isa and dyn_cast.
    static bool classof(const FunctionCallExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionCallExpr;
    }

private:
    SubroutineRef *connective;
    Expr **arguments;
    unsigned numArgs;
    Qualifier *qualifier;

    void setTypeForConnective();
};

//===----------------------------------------------------------------------===//
// IndexedArrayExpr
//
// Represents the indexing into an array expression.
class IndexedArrayExpr : public Expr {

public:
    IndexedArrayExpr(DeclRefExpr *arrExpr, Expr **indices, unsigned numIndices);

    ///@{
    /// Returns the expression denoting the array to index.
    DeclRefExpr *getArrayExpr() { return indexedArray; }
    const DeclRefExpr *getArrayExpr() const { return indexedArray; }
    ///@}

    /// Returns the number of indicies serving as subscripts.
    unsigned getNumIndices() const { return numIndices; }

    ///@{
    /// Returns the i'th index expression.
    Expr *getIndex(unsigned i) {
        assert(i < numIndices && "Index out of range!");
        return indexExprs[i];
    }

    const Expr *getIndex(unsigned i) const {
        assert(i < numIndices && "Index out of range!");
        return indexExprs[i];
    }
    ///@}

    ///@{
    ///
    /// Iterators over the index expressions.
    typedef Expr **index_iterator;
    index_iterator begin_indices() { return &indexExprs[0]; }
    index_iterator end_indices() { return &indexExprs[numIndices]; }

    typedef Expr *const *const_index_iterator;
    const_index_iterator begin_indices() const { return &indexExprs[0]; }
    const_index_iterator end_indices() const { return &indexExprs[numIndices]; }
    ///@}

    // Support isa and dyn_cast.
    static bool classof(const IndexedArrayExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IndexedArrayExpr;
    }

private:
    /// FIXME: The overwhelming majority of array index expressions will involve
    /// only a single dimension.  The representation should be optimized for
    /// this case.
    DeclRefExpr *indexedArray;
    unsigned numIndices;
    Expr **indexExprs;
};

//===----------------------------------------------------------------------===//
// InjExpr
//
// Represents "inj" expressions, mapping domain types to their carrier types.
class InjExpr : public Expr
{
public:
    InjExpr(Expr *argument, Type *resultType, Location loc)
        : Expr(AST_InjExpr, resultType, loc),
          operand(argument) { }

    Expr *getOperand() { return operand; }
    const Expr *getOperand() const { return operand; }

    static bool classof(const InjExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_InjExpr;
    }

private:
    Expr *operand;
};

//===----------------------------------------------------------------------===//
// PrjExpr
//
// Represents "prj" expressions, mapping carrier types to their domains.
class PrjExpr : public Expr
{
public:
    PrjExpr(Expr *argument, DomainType *resultType, Location loc)
        : Expr(AST_PrjExpr, resultType, loc),
          operand(argument) { }

    Expr *getOperand() { return operand; }
    const Expr *getOperand() const { return operand; }

    static bool classof(const PrjExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PrjExpr;
    }

private:
    Expr *operand;
};

//===----------------------------------------------------------------------===//
// IntegerLiteral
//
// Initially, IntegerLiteral nodes do not have an associated type.  The expected
// use case is that the node is created and the type refined once the context
// has been analyzed.
class IntegerLiteral : public Expr
{
public:
    IntegerLiteral(const llvm::APInt &value, Location loc)
        : Expr(AST_IntegerLiteral, loc), value(value) { }

    const llvm::APInt &getValue() const { return value; }

    void setValue(const llvm::APInt &V) { value = V; }

    static bool classof(const IntegerLiteral *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerLiteral;
    }

private:
    llvm::APInt value;
};

} // End comma namespace.

#endif
