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
#include "comma/ast/Type.h"
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
// Qualifier
//
// This little helper class used to represent qualifiers such as "D::" or
// "D(T)::" where D is a domain, functor, or a domain alias (such as a carrier
// type).  Note that this class is not a member of the Expr hierarchy, it is
// simply a common component of Expr nodes.
class Qualifier : public Ast {

public:
    Qualifier(Type *qualifier,  Location loc)
        : Ast(AST_Qualifier) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    void addQualifier(Type *qualifier, Location loc) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    unsigned numQualifiers() const { return qualifiers.size(); }

    typedef std::pair<Type*, Location> QualPair;

    QualPair getQualifier(unsigned n) const {
        assert(n < numQualifiers() && "Index out of range!");
        return qualifiers[n];
    }

    QualPair getBaseQualifier() const { return qualifiers.back(); }

private:
    typedef llvm::SmallVector<QualPair, 2> QualVector;
    QualVector  qualifiers;

public:
    typedef QualVector::iterator iterator;
    iterator begin() { return qualifiers.begin(); }
    iterator end()   { return qualifiers.end(); }

    typedef QualVector::const_iterator const_iterator;
    const_iterator begin() const { return qualifiers.begin(); }
    const_iterator end()   const { return qualifiers.end(); }

    static bool classof(const Qualifier *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_Qualifier;
    }
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
//
// There are two types of function call expressions -- ambiguous and
// unambiguous.  An ambiguous function call is one where the arguments types are
// fully resolved and known but there are multiple choices in effect wrt return
// types.  An ambiguous call expression maintains a set of function declarations
// which represent the possible candidates this call can resolve to.  Semantic
// analysis resolves these ambiguous call nodes once the type context of the
// call has been determined.
//
// Note that ambiguous function calls do not have an associated type, and
// getType will assert when called.
class FunctionCallExpr : public Expr {

public:
    FunctionCallExpr(FunctionDecl *connective,
                     Expr        **arguments,
                     unsigned      numArgs,
                     Location      loc);

    ~FunctionCallExpr();

    // Sets the qualifier to this call node, describing the source context of
    // its invocation.
    void setQualifier(Qualifier *qualifier) { this->qualifier = qualifier; }

    // Returns true if this function call is qualified.
    bool isQualified() const { return qualifier != 0; }

    // Returns the qualifier associated with this node, or NULL if no qualifier
    // has been set.
    Qualifier *getQualifier() { return qualifier; }

    // Returns the qualifier associated with this node, or NULL if no qualifier
    // has been set.
    const Qualifier *getQualifier() const { return qualifier; }

    // Adds a connective to this call expression.  This always results in this
    // call becoming ambiguous.  To resolve an ambiguous call, or to simply
    // change the associated connective, use FunctionCallExpr::setConnective
    // instead.
    void addConnective(FunctionDecl *connective);

    // Sets the connective for this call.  This always `resolves' the call
    // expression, making it unambiguous.  Use FunctionCallExpr::addConnective
    // to extend the set of connectives associated with this call.
    void setConnective(FunctionDecl *connective) {
        this->connective = connective;
        this->setType(connective->getReturnType());
    }

    // Returns true if this call is ambiguous.
    bool isAmbiguous() const {
        return connectiveBits & AMBIGUOUS_BIT;
    }

    // Returns true if this call is unambiguous.
    bool isUnambiguous() const {
        return !isAmbiguous();
    }

    // Returns the number of connectives associated with this call.  The result
    // is always greater than or equal to one.
    unsigned numConnectives() const;

    // Returns the \p i'th connective associated with this call.
    FunctionDecl *getConnective(unsigned i) const;

    // Forward iterator over the set of connectives associated with a function
    // call expression.
    class ConnectiveIterator {

        const FunctionCallExpr *callExpr;
        unsigned index;

    public:
        // Creates a sentinal iterator.
        ConnectiveIterator() :
            callExpr(0),
            index(0) { }

        // Creates an iterator over the connectives of the given call
        // expression.
        ConnectiveIterator(const FunctionCallExpr *callExpr)
            : callExpr(callExpr),
              index(0) { }

        ConnectiveIterator(const ConnectiveIterator &iter)
            : callExpr(iter.callExpr),
              index(iter.index) { }

        FunctionDecl *operator *() {
            assert(callExpr && "Cannot dereference an empty iterator!");
            return callExpr->getConnective(index);
        }

        bool operator ==(const ConnectiveIterator &iter) const {
            return (this->callExpr == iter.callExpr &&
                    this->index == iter.index);
        }

        bool operator !=(const ConnectiveIterator &iter) const {
            return !this->operator==(iter);
        }

        ConnectiveIterator &operator ++() {
            if (++index == callExpr->numConnectives()) {
                callExpr = 0;
                index    = 0;
            }
            return *this;
        }

        ConnectiveIterator operator ++(int) {
            ConnectiveIterator tmp = *this;
            this->operator++();
            return tmp;
        }
    };

    ConnectiveIterator beginConnectives() {
        return ConnectiveIterator(this);
    }

    ConnectiveIterator endConnectives() {
        return ConnectiveIterator();
    }

    // Returns the number of arguments supplied to this call expression.
    unsigned getNumArgs() const { return numArgs; }

    Expr *getArg(unsigned i) {
        assert(i < numArgs && "Index out of range!");
        return arguments[i];
    }

    void setArg(Expr *expr, unsigned i) {
        assert(i < numArgs && "Index out of range!");
        arguments[i] = expr;
    }

    static bool classof(const FunctionCallExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionCallExpr;
    }

private:
    enum { AMBIGUOUS_BIT = 1 };

    // The type used to store multiple connective declarations.
    typedef llvm::SmallVector<FunctionDecl*, 2> OptionVector;

    union {
        uintptr_t     connectiveBits;
        FunctionDecl *connective;
        OptionVector *connectiveOptions;
    };

    Expr     **arguments;
    unsigned   numArgs;
    Qualifier *qualifier;

    // Returns a clean pointer to the options vector.  Can only be called on an
    // ambiguous decl.
    OptionVector *getOptions() const {
        assert(isAmbiguous() &&
               "Only ambiguous decls have optional connectives!");
        return reinterpret_cast<OptionVector*>(connectiveBits & ~AMBIGUOUS_BIT);
    }
};

} // End comma namespace.

#endif
