//===-- ast/Stmt.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstBase.h"
#include "comma/ast/DeclRegion.h"
#include "llvm/ADT/SmallVector.h"

#ifndef COMMA_AST_STMT_HDR_GUARD
#define COMMA_AST_STMT_HDR_GUARD

namespace comma {

//===----------------------------------------------------------------------===//
// Stmt
class Stmt : public Ast {

protected:
    Stmt(AstKind kind) : Ast(kind) { }

public:
    static bool classof(const Stmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesStmt();
    }
};

//===----------------------------------------------------------------------===//
// StmtSequence
//
// Represents a sequence of statements.
class StmtSequence : public Stmt {

    llvm::SmallVector<Stmt*, 16> statements;

protected:
    StmtSequence(AstKind kind) : Stmt(kind) { }

public:
    StmtSequence() : Stmt(AST_StmtSequence) { }

    /// Construct a statement sequence given a pair of Stmt producing iterators.
    template <class Iter>
    StmtSequence(Iter I, Iter E)
        : Stmt(AST_StmtSequence),
          statements(I, E) { }

    void addStmt(Stmt *stmt) { statements.push_back(stmt); }

    template <class Iter>
    void addStmts(Iter I, Iter E) {
        for ( ; I != E; ++I)
            statements.push_back(*I);
    }

    /// Returns the number of statements contained in this sequence.
    unsigned size() const { return statements.size(); }

    typedef llvm::SmallVector<Stmt*, 16>::iterator StmtIter;
    StmtIter beginStatements() { return statements.begin(); }
    StmtIter endStatements()   { return statements.end(); }

    typedef llvm::SmallVector<Stmt*, 16>::const_iterator ConstStmtIter;
    ConstStmtIter beginStatements() const { return statements.begin(); }
    ConstStmtIter endStatements()   const { return statements.end(); }

    static bool classof(const StmtSequence *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_StmtSequence ||
            node->getKind() == AST_BlockStmt;
    }
};

//===----------------------------------------------------------------------===//
// BlockStmt
//
// Represents a block statement consisting of an optional identifier, a possibly
// empty declarative region, and a sequence of statements constituting the body.
class BlockStmt : public StmtSequence, public DeclRegion {

public:
    BlockStmt(Location        loc,
              DeclRegion     *parent,
              IdentifierInfo *label = 0)
        : StmtSequence(AST_BlockStmt),
          DeclRegion(AST_BlockStmt, parent),
          location(loc),
          label(label) { }

    // Returns true if this block has an associated label.
    bool hasLabel() const { return label != 0; }

    // Returns the label associated with this block, or 0 if there is no such
    // label.
    IdentifierInfo *getLabel() { return label; }

    Location getLocation() { return location; }

    static bool classof(const BlockStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_BlockStmt;
    }

private:
    Location        location;
    IdentifierInfo *label;
};

//===----------------------------------------------------------------------===//
// ProcedureCallStmt
//
// Representation of a procedure call statement.
class ProcedureCallStmt : public Stmt {

public:
    ProcedureCallStmt(ProcedureDecl *connective,
                      Expr **positionalArgs, unsigned numPositional,
                      KeywordSelector **keyedArgs, unsigned numKeys,
                      Location loc);

    ~ProcedureCallStmt();

    /// Returns the location of this procedure call.
    Location getLocation() const { return location; }

    /// Returns the procedure declaration underlying this call.
    ProcedureDecl *getConnective() const { return connective; }

    /// Returns the total number of arguments (positional and keyed) supplied to
    /// this procedure call.
    unsigned getNumArgs() const { return numArgs; }

    /// Returns the number of positional arguments supplied to this procedure
    /// call.
    unsigned getNumPositionalArgs() const {
        return getNumArgs() - getNumKeyedArgs();
    }

    /// Returns the number of keyed arguments supplied to this procedure call.
    unsigned getNumKeyedArgs() const { return numKeys; }

    /// \name Argument Iterators.
    ///
    /// \brief Iterators over all arguments of a procedure call.
    ///
    /// An arg_iterator is used to tarverse the full set of argument expressions
    /// in the order expected by the calls connective.  In other words, any
    /// keyed argument expressions are presented in an order consistent with the
    /// underlying connective, not in the order as originally supplied to the
    /// call.
    ///
    /// Unlike the argument iterators of a FunctionCallExpr, procedure calls are
    /// never ambiguous (they are always fully resolved based on their argument
    /// types).
    //@{
    typedef Expr **arg_iterator;
    arg_iterator begin_arguments() {
        return arguments ? &arguments[0] : 0;
    }
    arg_iterator end_arguments() {
        return arguments ? &arguments[numArgs] : 0;
    }

    typedef const Expr *const *const_arg_iterator;
    const_arg_iterator begin_arguments() const {
        return arguments ? &arguments[0] : 0;
    }
    const_arg_iterator end_arguments() const {
        return arguments ? &arguments[numArgs] : 0;
    }
    //@}

    /// \name Positional Argument Iterators.
    ///
    /// \brief Iterators over the positional arguments of a procedure call
    /// expression.
    //@{
    typedef Expr **pos_iterator;
    pos_iterator begin_positional() {
        return arguments ? &arguments[0] : 0;
    }
    pos_iterator end_positional() {
        return arguments ? &arguments[getNumPositionalArgs()] : 0;
    }

    typedef Expr *const *const_pos_iterator;
    const_pos_iterator begin_positional() const {
        return arguments ? &arguments[0] : 0;
    }
    const_pos_iterator end_positional() const {
        return arguments ? &arguments[getNumPositionalArgs()] : 0;
    }
    //@}

    /// \name KeywordSelector Iterators.
    ///
    /// \brief Iterators over the keyword selectors of this call expression.
    ///
    /// These iterators provide the keyword selectors in the order as they were
    /// originally supplied to the constructor.
    //@{
    typedef KeywordSelector **key_iterator;
    key_iterator begin_keys() { return keyedArgs ? &keyedArgs[0] : 0; }
    key_iterator end_keys() { return keyedArgs ? &keyedArgs[numKeys] : 0; }

    typedef KeywordSelector *const *const_key_iterator;
    const_key_iterator begin_keys() const {
        return keyedArgs ? &keyedArgs[0] : 0;
    }
    const_key_iterator end_keys() const {
        return keyedArgs ? &keyedArgs[numKeys] : 0;
    }
    //@}

    static bool classof(const ProcedureCallStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureCallStmt;
    }

private:
    ProcedureDecl *connective;
    Expr **arguments;
    KeywordSelector **keyedArgs;
    unsigned numArgs;
    unsigned numKeys;
    Location location;
};

//===----------------------------------------------------------------------===//
// ReturnStmt.
class ReturnStmt : public Stmt {

    Expr    *returnExpr;
    Location location;

public:
    ReturnStmt(Location loc, Expr *expr = 0)
        : Stmt(AST_ReturnStmt), returnExpr(expr), location(loc) { }

    ~ReturnStmt();

    bool hasReturnExpr() const { return returnExpr != 0; }

    const Expr *getReturnExpr() const { return returnExpr; }
    Expr *getReturnExpr() { return returnExpr; }


    Location getLocation() const { return location; }

    static bool classof(const ReturnStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ReturnStmt;
    }
};

//===----------------------------------------------------------------------===//
// AssignmentStmt
class AssignmentStmt : public Stmt {

    DeclRefExpr *target;
    Expr        *value;

public:
    AssignmentStmt(DeclRefExpr *target, Expr *value)
        : Stmt(AST_AssignmentStmt), target(target), value(value) { }

    DeclRefExpr *getTarget() { return target; }
    const DeclRefExpr *getTarget() const { return target; }

    Expr *getAssignedExpr() { return value; }
    const Expr *getAssignedExpr() const { return value; }

    static bool classof(const AssignmentStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AssignmentStmt;
    }
};

//===----------------------------------------------------------------------===//
// IfStmt
class IfStmt : public Stmt {

public:
    // IfStmt's are always constructed with a condition and a consequent.  If
    // the statement contains "elsif" components, one must call addElsif for
    // each component.  Similarly, one must call setAlternate to define the
    // "else" component.
    IfStmt(Location loc, Expr *condition, StmtSequence *consequent)
        : Stmt(AST_IfStmt),
          ifLocation(loc),
          elseLocation(0),
          condition(condition),
          consequent(consequent),
          alternate(0) { }

    // Returns the predicate expression controlling this IfStmt.
    Expr *getCondition() { return condition; }
    const Expr *getCondition() const { return condition; }

    // Returns the statement associated with the "then" branch of this IfStmt.
    StmtSequence *getConsequent() { return consequent; }
    const StmtSequence *getConsequent() const { return consequent; }

    // Sets the statement associated with the "else" branch of this IfStmt.
    void setAlternate(Location loc, StmtSequence *stmt) {
        assert(alternate == 0 &&  "Cannot reset IfStmt alternate!");
        elseLocation = loc;
        alternate    = stmt;
    }

    // Returns true if this IfStmt has been supplied with an "else" clause.
    bool hasAlternate() const { return alternate != 0; }

    // Returns the statement associated with the "else" clause, or 0 if no such
    // component exists.
    StmtSequence *getAlternate() { return alternate; }
    const StmtSequence *getAlternate() const { return alternate; }

    // The following class is used to represent "elsif" components of a
    // conditional.
    class Elsif {

    public:
        Location getLocation() const { return location; }

        Expr *getCondition() { return condition; }
        const Expr *getCondition() const { return condition; }

        StmtSequence *getConsequent() { return consequent; }
        const StmtSequence *getConsequent() const { return consequent; }

    private:
        Elsif(Location loc, Expr *cond, StmtSequence *stmt)
            : location(loc), condition(cond), consequent(stmt) { }

        friend class IfStmt;

        Location      location;
        Expr         *condition;
        StmtSequence *consequent;
    };

private:
    // The type used to store Elsif components.
    typedef llvm::SmallVector<Elsif, 2> ElsifVector;

public:
    typedef ElsifVector::iterator       iterator;
    typedef ElsifVector::const_iterator const_iterator;

    iterator beginElsif() { return elsifs.begin(); }
    iterator endElsif()   { return elsifs.end(); }

    const_iterator beginElsif() const { return elsifs.begin(); }
    const_iterator endElsif()   const { return elsifs.end(); }

    // Adds an "elsif" branch to this IfStmt.  The order in which this function
    // is called determines the order of the elsif branches.
    void addElsif(Location loc, Expr *condition, StmtSequence *consequent) {
        elsifs.push_back(Elsif(loc, condition, consequent));
    }

    // Returns true if this if statement contains elsif clauses.
    bool hasElsif() const { return !elsifs.empty(); }

    // Returns the location of the "if" token.
    Location getIfLocation() const { return ifLocation; }

    // Returns the location of the "else" token if an alternate branch exists.
    Location getElseLocation() const { return elseLocation; }

    static bool classof(const IfStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IfStmt;
    }

private:
    Location      ifLocation;
    Location      elseLocation;
    Expr         *condition;
    StmtSequence *consequent;
    StmtSequence *alternate;
    ElsifVector   elsifs;
};

//===----------------------------------------------------------------------===//
// WhileStmt
//
// Ast nodes representing the 'while' loop construct.
class WhileStmt : public Stmt {

public:
    WhileStmt(Location loc, Expr *condition, StmtSequence *body)
        : Stmt(AST_WhileStmt),
          location(loc),
          condition(condition),
          body(body) { }

    // Returns the condition expression controlling this loop.
    Expr *getCondition() { return condition; }
    const Expr *getCondition() const { return condition; }

    // Returns the body of this loop.
    StmtSequence *getBody() { return body; }
    const StmtSequence *getBody() const { return body; }

    // Returns the location of the 'while' reserved word starting this loop.
    Location getLocation() { return location; }

    static bool classof(const WhileStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_WhileStmt;
    }

private:
    Location location;
    Expr *condition;
    StmtSequence *body;
};

} // End comma namespace.

#endif
