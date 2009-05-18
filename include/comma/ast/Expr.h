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
    Qualifier(DeclRegion *qualifier,  Location loc)
        : Ast(AST_Qualifier) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    void addQualifier(DeclRegion *qualifier, Location loc) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    unsigned numQualifiers() const { return qualifiers.size(); }

    typedef std::pair<DeclRegion*, Location> QualPair;

    QualPair getQualifier(unsigned n) const {
        assert(n < numQualifiers() && "Index out of range!");
        return qualifiers[n];
    }

    // Returns the base (most specific) declarative region of this qualifier.
    DeclRegion *resolve() {
        return qualifiers.back().first;
    }

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

class FunctionCallExpr : public Expr {

public:
    FunctionCallExpr(Decl      *connective,
                     Expr     **arguments,
                     unsigned   numArgs,
                     Location   loc);

    FunctionCallExpr(Decl     **connectives,
                     unsigned   numConnectives,
                     Expr     **arguments,
                     unsigned   numArgs,
                     Location   loc);

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

    // Returns the connective associcated with this call.  The resulting node is
    // either a FunctionDecl, EnumerationLiteral, or OverloadedDeclName.
    Ast *getConnective();

    // Resolved the connective for this call.  This method can only be called
    // when the call is currently ambiguous.
    void resolveConnective(FunctionDecl *connective);

    // Resolved the connective for this call.  This method can only be called
    // when the call is currently ambiguous.
    void resolveConnective(EnumLiteral *connective);

    // Resolved the connective for this call.  This method can only be called
    // when the call is currently ambiguous.  This method will assert if its
    // argument is not a FunctionDecl or EnumLiteral.
    void resolveConnective(Decl *connective);

    // Returns true if this call is ambiguous.
    bool isAmbiguous() const {
        return llvm::isa<OverloadedDeclName>(connective);
    }

    // Returns true if this call is unambiguous.
    bool isUnambiguous() const {
        return !isAmbiguous();
    }

    // Returns true if this call contains a connective with the given type.
    bool containsConnective(FunctionType *) const;

    // Returns the number of connectives associated with this call.  The result
    // is always greater than or equal to one.
    unsigned numConnectives() const;

    // Returns the \p i'th connective associated with this call.  The returned
    // node is either an EnumLiteral or FunctionDecl.
    Decl *getConnective(unsigned i) const;

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

        Decl *operator *() {
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
    Ast       *connective;
    Expr     **arguments;
    unsigned   numArgs;
    Qualifier *qualifier;

    void setTypeForConnective();
};

//===----------------------------------------------------------------------===//
// InjExpr
//
// Represents "inj" expressions, mapping domain types to their carrier types.
class InjExpr : public Expr
{
public:
    InjExpr(Expr *argument, Type *resultType, Location loc)
        : Expr(AST_InjExpr, resultType, loc) { }

    static bool classof(const InjExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_InjExpr;
    }
};

//===----------------------------------------------------------------------===//
// PrjExpr
//
// Represents "prj" expressions, mapping carrier types to their domains.
class PrjExpr : public Expr
{
public:
    PrjExpr(Expr *argument, DomainType *resultType, Location loc)
        : Expr(AST_PrjExpr, resultType, loc) { }

    static bool classof(const PrjExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PrjExpr;
    }
};

} // End comma namespace.

#endif
