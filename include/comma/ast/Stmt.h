//===-- ast/Stmt.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_STMT_HDR_GUARD
#define COMMA_AST_STMT_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/DeclRegion.h"
#include "comma/ast/SubroutineCall.h"

namespace comma {

class Pragma;

//===----------------------------------------------------------------------===//
// Stmt
class Stmt : public Ast {

protected:
    Stmt(AstKind kind, Location loc) : Ast(kind), location(loc) { }

public:
    /// Returns the location of this statement.
    Location getLocation() const { return location; }

    /// Returns true if this statement represents a \c return a \c raise or an
    /// \c exit statement that is not predicated over a condition.
    bool isTerminator() const;

    // Support isa/dyn_cast.
    static bool classof(const Stmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesStmt();
    }

private:
    Location location;
};

//===----------------------------------------------------------------------===//
// StmtSequence
//
// Represents a sequence of statements.
class StmtSequence : public Stmt {

    typedef llvm::SmallVector<Stmt*, 16> StatementVec;
    typedef llvm::SmallVector<HandlerStmt*, 2> HandlerVec;
    StatementVec statements;
    HandlerVec handlers;

protected:
    StmtSequence(AstKind kind, Location loc) : Stmt(kind, loc) { }

public:
    StmtSequence(Location loc) : Stmt(AST_StmtSequence, loc) { }

    /// Construct a statement sequence given a pair of Stmt producing iterators.
    template <class Iter>
    StmtSequence(Location loc, Iter I, Iter E)
        : Stmt(AST_StmtSequence, loc),
          statements(I, E) { }

    /// Adds a single statement to the end of this sequence.
    void addStmt(Stmt *stmt) { statements.push_back(stmt); }

    /// Adds the given set of statements provided by an iterator pair to the end
    /// of this sequence.
    template <class Iter>
    void addStmts(Iter I, Iter E) {
        for ( ; I != E; ++I)
            statements.push_back(*I);
    }

    /// Returns the number of statements contained in this sequence.
    unsigned numStatements() const { return statements.size(); }

    /// Returns true if this statement sequence is empty.
    bool empty() const { return numStatements() == 0; }

    //@{
    /// Returns the first statement in this sequence.
    Stmt *front() { return statements.front(); }
    const Stmt *front() const { return statements.front(); }
    //@}

    //@{
    /// Returns the last statement in this sequence.
    Stmt *back() { return statements.back(); }
    const Stmt *back() const { return statements.back(); }
    //@}

    //@{
    /// Iterators over the statements provided by this StmtSequence.
    typedef StatementVec::iterator stmt_iter;
    stmt_iter stmt_begin() { return statements.begin(); }
    stmt_iter stmt_end()   { return statements.end(); }

    typedef StatementVec::const_iterator const_stmt_iter;
    const_stmt_iter stmt_begin() const { return statements.begin(); }
    const_stmt_iter stmt_end()   const { return statements.end(); }
    //@}

    /// Returns true if there are any exception handlers associated with this
    /// StmtSequence.
    bool isHandled() const { return !handlers.empty(); }

    /// Returns the number of handlers asscociated with this StmtSequence.
    unsigned numHandlers() const { return handlers.size(); }

    /// Returns true if there is an "others" catch-all handler associated with
    /// this sequence.
    bool hasCatchAll() const;

    /// Returns true if the given ExceptionDecl is covered by any of the
    /// handlers in this sequence.
    ///
    /// \note This method will always return false when isHandled() is false,
    /// and always return true when hasCatchAll() is true.
    bool handles(const ExceptionDecl *exception) const;

    /// Adds a handler to this StmtSequence.
    ///
    /// If the given handler is a "catch-all" handler, this must be the last
    /// handler registered with this sequence.
    void addHandler(HandlerStmt *handler) {
        assert(!hasCatchAll() && "Catch-all handler already present!");
        handlers.push_back(handler);
    }

    //@{
    /// Iterators over the handlers provided by this StmtSequence.
    typedef HandlerVec::iterator handler_iter;
    handler_iter handler_begin() { return handlers.begin(); }
    handler_iter handler_end() { return handlers.end(); }

    typedef HandlerVec::const_iterator const_handler_iter;
    const_handler_iter handler_begin() const { return handlers.begin(); }
    const_handler_iter handler_end() const { return handlers.end(); }
    //@}

    // Support isa/dyn_cast
    static bool classof(const StmtSequence *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_StmtSequence || kind == AST_BlockStmt ||
            kind == AST_HandlerStmt;
    }
};

//===----------------------------------------------------------------------===//
// HandlerStmt
//
/// HandlerStmt nodes represent an exception handler.
class HandlerStmt : public StmtSequence {

public:
    /// Constructs a HandlerStmt over the given set of execption choices.
    ///
    /// If \p numChoices is zero, then the resulting handler is considered a
    /// "catch-all", corresponding to the code <tt>when others</tt>.
    HandlerStmt(Location loc, ExceptionRef **choices, unsigned numChoices);

    /// Returns the number of exception choices associated with this handlers.
    unsigned getNumChoices() const { return numChoices; }

    /// Returns true if this handler denotes a "catch-all".
    bool isCatchAll() const { return getNumChoices() == 0; }

    /// Returns true if this handler covers the given exception.
    bool handles(const ExceptionDecl *exception) const;

    //@{
    /// Iterators over the set of exception choices associated with this
    /// handler.
    typedef ExceptionRef **choice_iterator;
    choice_iterator choice_begin() { return choices; }
    choice_iterator choice_end() { return choices + numChoices; }

    typedef const ExceptionRef *const *const_choice_iterator;
    const_choice_iterator choice_begin() const { return choices; }
    const_choice_iterator choice_end() const { return choices + numChoices; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const HandlerStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_HandlerStmt;
    }

private:
    unsigned numChoices;
    ExceptionRef **choices;
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
        : StmtSequence(AST_BlockStmt, loc),
          DeclRegion(AST_BlockStmt, parent),
          label(label) { }

    // Returns true if this block has an associated label.
    bool hasLabel() const { return label != 0; }

    // Returns the label associated with this block, or 0 if there is no such
    // label.
    IdentifierInfo *getLabel() { return label; }

    // Support isa/dyn_cast.
    static bool classof(const BlockStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_BlockStmt;
    }

private:
    IdentifierInfo *label;
};

//===----------------------------------------------------------------------===//
// ProcedureCallStmt
//
// Representation of a procedure call statement.
class ProcedureCallStmt : public Stmt, public SubroutineCall {

public:
    ProcedureCallStmt(SubroutineRef *ref,
                      Expr **positionalArgs, unsigned numPositional,
                      KeywordSelector **keyedArgs, unsigned numKeys);

    /// Returns the location of this procedure call statement.
    Location getLocation() const {
        // Required since SubroutineCall::getLocation is pure virtual.
        return Stmt::getLocation();
    }

    //@{
    /// Returns the procedure declaration underlying this call.
    const ProcedureDecl *getConnective() const {
        return llvm::cast<ProcedureDecl>(SubroutineCall::getConnective());
    }
    ProcedureDecl *getConnective() {
        return llvm::cast<ProcedureDecl>(SubroutineCall::getConnective());
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const ProcedureCallStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureCallStmt;
    }
};

//===----------------------------------------------------------------------===//
// ReturnStmt.
class ReturnStmt : public Stmt {

public:
    ReturnStmt(Location loc, Expr *expr = 0)
        : Stmt(AST_ReturnStmt, loc), returnExpr(expr) { }

    ~ReturnStmt();

    bool hasReturnExpr() const { return returnExpr != 0; }

    const Expr *getReturnExpr() const { return returnExpr; }
    Expr *getReturnExpr() { return returnExpr; }

    static bool classof(const ReturnStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ReturnStmt;
    }

private:
    Expr *returnExpr;
};

//===----------------------------------------------------------------------===//
// AssignmentStmt
class AssignmentStmt : public Stmt {

public:
    AssignmentStmt(Expr *target, Expr *value);

    Expr *getTarget() { return target; }
    const Expr *getTarget() const { return target; }

    Expr *getAssignedExpr() { return value; }
    const Expr *getAssignedExpr() const { return value; }

    // Support isa/dyn_cast.
    static bool classof(const AssignmentStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AssignmentStmt;
    }

private:
    Expr *target;
    Expr *value;
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
        : Stmt(AST_IfStmt, loc),
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
    Location getIfLocation() const { return Stmt::getLocation(); }

    // Returns the location of the "else" token if an alternate branch exists.
    Location getElseLocation() const { return elseLocation; }

    static bool classof(const IfStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IfStmt;
    }

private:
    Location      elseLocation;
    Expr         *condition;
    StmtSequence *consequent;
    StmtSequence *alternate;
    ElsifVector   elsifs;
};

//===----------------------------------------------------------------------===//
// IterationStmt
//
/// \class
///
/// \brief Common base class for all looping statements.
class IterationStmt : public Stmt {

public:
    virtual ~IterationStmt() { }

    //@{
    /// Returns the body of this iteration statemement.
    StmtSequence *getBody() { return &body; }
    const StmtSequence *getBody() const { return &body; }
    //@}

    /// Returns true if this iteration statement is tagged.
    bool isTagged() const { return tag != 0; }

    /// Returns the tag identifying this iteration statement, or null is this is
    /// untagged.
    IdentifierInfo *getTag() const { return tag; }

    /// Returns the location of this loops tag.
    Location getTagLocation() const { return tagLoc; }

    /// Sets the tag and tag location for this iteration statement,
    void setTag(IdentifierInfo *tag, Location tagLoc) {
        this->tag = tag;
        this->tagLoc = tagLoc;
    }

    // Support isa/dyn_cast.
    static bool classof(const IterationStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return denotesIterationStmt(node->getKind());
    }

protected:
    IterationStmt(AstKind kind, Location loc)
        : Stmt(kind, loc), tag(0), body(loc) {
        assert(denotesIterationStmt(kind));
    }

    IdentifierInfo *tag;
    Location tagLoc;
    StmtSequence body;

private:
    static bool denotesIterationStmt(AstKind kind) {
        return (kind == AST_ForStmt || kind == AST_WhileStmt ||
                kind == AST_LoopStmt);
    }
};

//===----------------------------------------------------------------------===//
// WhileStmt
//
// Ast nodes representing the 'while' loop construct.
class WhileStmt : public IterationStmt {

public:
    WhileStmt(Location loc, Expr *condition)
        : IterationStmt(AST_WhileStmt, loc),
          condition(condition) { }

    //@{
    /// Returns the condition expression controlling this loop.
    Expr *getCondition() { return condition; }
    const Expr *getCondition() const { return condition; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const WhileStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_WhileStmt;
    }

private:
    Expr *condition;
};

//===----------------------------------------------------------------------===//
// ForStmt
//
/// This node represents the "for" loop iteration scheme.
class ForStmt : public IterationStmt {

public:
    /// \brief Constructs a for-loop statement over the given declaration and
    /// discrete subtype definition node.
    ForStmt(Location loc, LoopDecl *iterationDecl, DSTDefinition *control);

    //@{
    /// Returns the LoopDecl corresponding to the iteration value of this loop.
    const LoopDecl *getLoopDecl() const { return iterationDecl; }
    LoopDecl *getLoopDecl() { return iterationDecl; }
    //@}

    //@{
    /// Returns the discrete subtype definition controlling this loop.
    const DSTDefinition *getControl() const { return control; }
    DSTDefinition *getControl() { return control; }
    //@}

    //@{
    /// Returns the controlling subtype of this loop.  All loop controls have an
    /// associated type (the type of the associated LoopDecl).
    const DiscreteType *getControlType() const {
        return getLoopDecl()->getType();
    }
    DiscreteType *getControlType() { return getLoopDecl()->getType(); }
    //@}

    /// Returns true if the controlling scheme is reversed.
    bool isReversed() const { return bits == 1; }

    /// Marks that this loop is reversed.
    void markAsReversed() { bits = 1; }

    // Support isa/dyn_cast.
    static bool classof(const ForStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ForStmt;
    }

private:
    LoopDecl *iterationDecl;
    DSTDefinition *control;
};

//===----------------------------------------------------------------------===//
// LoopStmt
//
/// This class represents the simple "loop" statement.
class LoopStmt : public IterationStmt {

public:
    LoopStmt(Location loc) : IterationStmt(AST_LoopStmt, loc) { }

    // Support isa/dyn_cast.
    static bool classof(const LoopStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_LoopStmt;
    }
};

//===----------------------------------------------------------------------===//
// ExitStmt
//
// Representation of exit statements.
class ExitStmt : public Stmt {

public:

    /// Builds a simple exit statement without a destination tag.  If a
    /// condition is to be associated with this statement node an additional
    /// call to setCondition is required.
    ExitStmt(Location loc) :
        Stmt(AST_ExitStmt, loc), tag(0), condition(0) { }

    /// Builds an exit statement with a destination tag.  If a condition is to
    /// be associated with this statement node an additional call to
    /// setCondition is required.
    ExitStmt(Location loc, IdentifierInfo *tag, Location tagLoc) :
        Stmt(AST_ExitStmt, loc), tag(tag), tagLoc(tagLoc), condition(0) { }

    /// Returns true if an exit tag is associated with this statement.
    bool hasTag() const { return tag != 0; }

    /// Returns the tag associated with this statement or null if hasTag is
    /// false.
    IdentifierInfo *getTag() const { return tag; }

    /// Returns the location of the exit tag, or an invalid location if a tag
    /// has not been associated.
    Location getTagLocation() const { return tagLoc; }

    /// Returns true if this exit node has an associated condition.
    bool hasCondition() const { return condition != 0; }

    /// Unconditionally sets the condition associated with this exit statement.
    void setCondition(Expr *expr) { condition = expr; }

    //@{
    /// Returns the condition associated with this exit statement or null if
    /// hasConsdition returns false.
    const Expr *getCondition() const { return condition; }
    Expr *getCondition() { return condition; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ExitStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ExitStmt;
    }

private:
    IdentifierInfo *tag;
    Location tagLoc;
    Expr *condition;
};

//===----------------------------------------------------------------------===//
// PragmaStmt
//
// This is a simple Stmt node which wraps a pragma so that it can appear within
// a sequence of statements.
class PragmaStmt : public Stmt {

public:
    PragmaStmt(Pragma *pragma);

    const Pragma *getPragma() const { return pragma; }
    Pragma *getPragma() { return pragma; }

    // Support isa/dyn_cast.
    static bool classof(const PragmaStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PragmaStmt;
    }

private:
    Pragma *pragma;
};

//===----------------------------------------------------------------------===//
// RaiseStmt
class RaiseStmt : public Stmt {

public:
    /// Constructs a RaiseStmt node.
    ///
    /// \param loc Location of the `raise' keyword.
    ///
    /// \param exception ExceptionRef corresponding to the exception to raise.
    ///
    /// \param message Optional expression of type String serving as the message
    /// to be attached to the exception.
    RaiseStmt(Location loc, ExceptionRef *exception, Expr *message = 0)
        : Stmt(AST_RaiseStmt, loc),
          ref(exception), message(message) { }

    /// Returns the associated exception declaration.
    //@{
    const ExceptionDecl *getExceptionDecl() const;
    ExceptionDecl *getExceptionDecl();
    //@}

    /// Returns the associated exception reference.
    //@{
    const ExceptionRef *getExceptionRef() const { return ref; }
    ExceptionRef *getExceptionRef() { return ref; }
    //@}

    /// Returns true if this raise statement has a message associated with it.
    bool hasMessage() const { return message != 0; }

    /// Returns the message associated with this raise statement, or null if
    /// there is none.
    //@{
    const Expr *getMessage() const { return message; }
    Expr *getMessage() { return message; }
    //@}

    /// Sets the message associated with this statement.
    void setMessage(Expr *message) { this->message = message; }

    /// Sets the exception reference associated with this statement.
    void setException(ExceptionRef *exception) { ref = exception; }

    // Support isa/dyn_cast.
    static bool classof(const RaiseStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_RaiseStmt;
    }

private:
    ExceptionRef *ref;
    Expr *message;
};

//===----------------------------------------------------------------------===//
// NullStmt
class NullStmt : public Stmt {

public:
    /// Constructs a null statement at the given location.
    NullStmt(Location loc) : Stmt(AST_NullStmt, loc) { }

    // Support isa/dyn_cast.
    static bool classof(const NullStmt *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_NullStmt;
    }
};

} // End comma namespace.

#endif
