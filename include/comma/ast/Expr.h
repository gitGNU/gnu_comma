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
                     Expr **positionalArgs, unsigned numPositional,
                     KeywordSelector **keyedArgs, unsigned numKeys);

    /// Creates a function call expression over the given set of function
    /// declarations and arguments.
    ///
    /// The given declarations must all have the same name, and otherwise be
    /// compatible with the construction of a SubroutineRef.
    FunctionCallExpr(FunctionDecl **connectives, unsigned numConnectives,
                     Expr **positionalArgs, unsigned numPositional,
                     KeywordSelector **keyedArgs, unsigned numKeys,
                     Location loc);

    /// Creates a function call expression over a single unique function
    /// declaration.  The resulting call expression is unambiguous.
    FunctionCallExpr(FunctionDecl *fdecl,
                     Expr **positionalArgs, unsigned numPositional,
                     KeywordSelector **keyedArgs, unsigned numKeys,
                     Location loc);

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
    /// If this call is unambiguous, returns the unique function declaration
    /// serving as the connective for this call, otherwise returns null.
    FunctionDecl *getConnective() {
        if (isUnambiguous()) {
            return llvm::cast<FunctionDecl>(connective->getDeclaration(0));
        }
        return 0;
    };

    const FunctionDecl *getConnective() const {
        if (isUnambiguous()) {
            return llvm::cast<FunctionDecl>(connective->getDeclaration(0));
        }
        return 0;
    }
    //@}

    /// Resolved the connective for this call.
    ///
    /// The supplied function declaration must accept the exact number of
    /// arguments this call supplies.  Furthermore, if this call was made with
    /// keyed arguments, the supplied declaration must accept the format of this
    /// call.  In particular:
    ///
    ///   - For each keyword argument supplied to this call, the declaration
    ///     must provide a matching keyword.
    ///
    ///   - The position of a keyed argument must be greater than the number of
    ///     positional parameters supplied to this call.
    ///
    /// Provided that the supplied connective meets these constraints, this call
    /// becomes unambiguous, and the full set of arguments becomes available
    /// thru the arg_iterator interface.
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

    /// Returns total the number of arguments supplied to this call expression.
    /// This is the sum of all positional and keyed arguments.
    unsigned getNumArgs() const { return numPositional + numKeys; }

    /// Returns the number of positional arguments supplied to this call
    /// expression.
    unsigned getNumPositionalArgs() const { return numPositional; }

    /// Returns the number of keyed arguments supplied to this call expression.
    unsigned getNumKeyedArgs() const { return numKeys; }

    /// \name Argument Iterators.
    ///
    /// \brief Iterators over the arguments of an unambiguous call expression.
    ///
    /// These iterators can be accessed only when this call expression is
    /// unambiguous, otherwise an assertion will be raised.
    ///
    /// An arg_iterator is used to traverse the full set of argument expressions
    /// in the order expected by the calls connective.  In other words, any
    /// keyed argument expressions are presented in an order consistent with the
    /// underlying connective, not in the order as originally supplied to
    /// call.
    //@{
    typedef Expr **arg_iterator;
    arg_iterator begin_arguments() {
        assert(isUnambiguous() &&
               "Cannot iterate over the arguments of an ambiguous call expr!");
        return arguments ? &arguments[0] : 0;
    }
    arg_iterator end_arguments() {
        assert(isUnambiguous() &&
               "Cannot iterate over the arguments of an ambiguous call expr!");
        return arguments ? &arguments[getNumArgs()] : 0;
    }

    typedef const Expr *const *const_arg_iterator;
    const_arg_iterator begin_arguments() const {
        assert(isUnambiguous() &&
               "Cannot iterate over the arguments of an ambiguous call expr!");
        return arguments ? &arguments[0] : 0;
    }
    const_arg_iterator end_arguments() const {
        assert(isUnambiguous() &&
               "Cannot iterate over the arguments of an ambiguous call expr!");
        return arguments ? &arguments[getNumArgs()] : 0;
    }

    //@}

    /// \name Positional Argument Iterators.
    ///
    /// \brief Iterators over the positional arguments of a call expression.
    ///
    /// Unlike the more specific arg_iterator, positional iterators are valid
    /// even when the call is ambiguous.
    //@{
    typedef Expr **pos_iterator;
    pos_iterator begin_positional() {
        return numPositional ? &arguments[0] : 0;
    }
    pos_iterator end_positional() {
        return numPositional ? &arguments[numPositional] : 0;
    }

    typedef Expr *const *const_pos_iterator;
    const_pos_iterator begin_positional() const {
        return numPositional ? &arguments[0] : 0;
    }
    const_pos_iterator end_positional() const {
        return numPositional ? &arguments[numPositional] : 0;
    }
    //@}

    /// \name KeywordSelector Iterators.
    ///
    /// \brief Iterators over the keyword selectors of this call expression.
    ///
    /// These iterators are valid even for ambiguous call expressions.  They
    /// provide the keyword selectors in the order as they were originally
    /// supplied to the constructor.
    //@{
    typedef KeywordSelector **key_iterator;
    key_iterator begin_keys() { return numKeys ? &keyedArgs[0] : 0; }
    key_iterator end_keys() { return numKeys ? &keyedArgs[numKeys] : 0; }

    typedef KeywordSelector *const *const_key_iterator;
    const_key_iterator begin_keys() const {
        return numKeys ? &keyedArgs[0] : 0;
    }
    const_key_iterator end_keys() const {
        return numKeys ? &keyedArgs[numKeys] : 0;
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const FunctionCallExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionCallExpr;
    }

private:
    SubroutineRef *connective;
    Expr **arguments;
    KeywordSelector **keyedArgs;
    unsigned numPositional;
    unsigned numKeys;
    Qualifier *qualifier;

    void setTypeForConnective();
    void setArguments(Expr **posArgs, unsigned numPos,
                      KeywordSelector **keyArgs, unsigned numKeys);
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
