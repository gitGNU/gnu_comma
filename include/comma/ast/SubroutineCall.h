//===-- ast/SubroutineCall.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the SubroutineCall class.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_SUBROUTINECALL_HDR_GUARD
#define COMMA_AST_SUBROUTINECALL_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/SubroutineRef.h"

namespace comma {

/// The SubroutineCall class is a mixin which provides functionality common to
/// both FunctionCallExpr and ProcedureCallStmt.  Both nodes share a great deal
/// of functionality but with one being an expression and the other being a
/// statement they cannot both inherit from a common base class in the AST
/// hierarchy -- hense this class.
class SubroutineCall {

public:
    /// The given SubroutineRef must be compatable with the concrete
    /// implementation of this class.  If the implementation is a
    /// FunctionCallExpr, then the reference must be to a collection of
    /// FunctionDecl's.
    ///
    /// If the reference contains more than one declaration, then this
    /// SubroutineCell is said to be ambiguous.
    ///
    /// SubroutineCall's take ownership of the connective and all arguments.
    SubroutineCall(SubroutineRef *connective,
                   Expr **positionalArgs, unsigned numPositional,
                   KeywordSelector **keyedArgs, unsigned numKeys);

    /// Constructs a subroutine call over a single connective.  This constructor
    /// always results in a fully resolved call node.
    SubroutineCall(SubroutineDecl *connective,
                   Expr **positionalArgs, unsigned numPositional,
                   KeywordSelector **keyedArgs, unsigned numKeys);

    virtual ~SubroutineCall();

    /// Returns true if this is a function call expression.
    bool isaFunctionCall() const;

    /// Returns true if this is a procedure call statement.
    bool isaProcedureCall() const;

    //@{
    /// Returns this as a FunctionCallExpr or null.
    FunctionCallExpr *asFunctionCall();
    const FunctionCallExpr *asFunctionCall() const;
    //@}

    //@{
    /// Returns this as a ProcedureCallStmt or null.
    ProcedureCallStmt *asProcedureCall();
    const ProcedureCallStmt *asProcedureCall() const;
    //@}

    /// Returns the location of this subroutine call.
    virtual Location getLocation() const = 0;

    /// Returns true if this call is ambiguous.
    bool isAmbiguous() const {
        return connective->isOverloaded() || connective->empty();
    }

    /// Returns true if this call is unambiguous.
    bool isUnambiguous() const { return !isAmbiguous(); }

    /// Returns true if this call is primitive.
    ///
    /// This method returns true iff the call has been resolved (that is,
    /// isUnambiguous() returns true), and if the connective of this call
    /// denotes a primitive operation.
    bool isPrimitive() const {
        return isUnambiguous() && getConnective()->isPrimitive();
    }

    /// \name Call Types.
    ///
    /// The following predicates provide a classification of calls from the
    /// perspective of the caller.  For any given \em unambiguous call node, one
    /// (and only one) of the following predicates returns true.  For an
    /// ambiguous call node \em all of the following predicates return false.
    //@{

    /// Returns true if this is a local call.
    ///
    /// A local call is one which references a subroutine declaration which is
    /// internal to a capsule.  A consequence of beeing local is that callee
    /// must be local to the capsule as well.
    bool isLocalCall() const;

    /// Returns true if this is a direct call.
    ///
    /// A direct call is one which references a subroutine declaration provided
    /// by a concrete (non-abstract) domain, and is not a local call.
    bool isDirectCall() const;

    /// Returns true if this is an abstract call.
    ///
    /// A call is abstract if it references a subroutine declaration provided by
    /// an abstract domain.
    bool isAbstractCall() const;

    /// Returns true if this is a foreign call.
    ///
    /// A foreign call is one which references a subroutine declaration which
    /// has an Import pragma attached to it.
    bool isForeignCall() const;
    //@}

    /// Resolved the connective for this call.
    ///
    /// The supplied subroutine declaration must accept the exact number of
    /// arguments this call supplies and be compatable with the type of call.
    /// Furthermore, if this call was made with keyed arguments, the supplied
    /// declaration must accept the format of this call.  In particular:
    ///
    ///   - For each keyword argument supplied to this call, the declaration
    ///     must provide a matching keyword.
    ///
    ///   - The formal position of a keyed argument must be greater than the
    ///     number of positional parameters supplied to this call.
    ///
    /// Provided that the supplied connective meets these constraints, this call
    /// becomes unambiguous, and the full set of arguments becomes available
    /// thru the arg_iterator interface.
    virtual void resolveConnective(SubroutineDecl *connective);

    /// Returns total the number of arguments supplied to this call.  This is
    /// the sum of all positional and keyed arguments.
    unsigned getNumArgs() const { return numPositional + numKeys; }

    /// Returns the number of positional arguments supplied to this call.
    unsigned getNumPositionalArgs() const { return numPositional; }

    /// Returns the number of keyed arguments supplied to this call.
    unsigned getNumKeyedArgs() const { return numKeys; }

    /// Returns true if this call contains a connective with the given type.
    bool containsConnective(SubroutineType *srTy) const {
        return connective->contains(srTy);
    }

    /// Returns the number of connectives associated with this call.
    unsigned numConnectives() const { return connective->numDeclarations(); }

    //@{
    /// Returns the \p i'th connective associated with this call.
    const SubroutineDecl *getConnective(unsigned i) const {
        return connective->getDeclaration(i);
    }
    SubroutineDecl *getConnective(unsigned i) {
        return connective->getDeclaration(i);
    }
    //@}

    //@{
    /// When this call is unambiguous, returns the unique subroutine declaration
    /// associated with this call.  If this call is ambiguous, an assertion will
    /// fire.
    SubroutineDecl *getConnective() {
        assert(isUnambiguous() &&
               "No unique connective associated with this call!");
        return connective->getDeclaration(0);
    }

    const SubroutineDecl *getConnective() const {
        assert(isUnambiguous() &&
               "No unique connective associated with this call!");
        return connective->getDeclaration(0);
    }
    //@}

    //@{
    /// Iterators over the set of connectives associcated with this call
    /// expression.
    typedef SubroutineRef::iterator connective_iterator;
    connective_iterator begin_connectives() { return connective->begin(); }
    connective_iterator end_connectives() { return connective->end(); }

    typedef SubroutineRef::const_iterator const_connective_iterator;
    const_connective_iterator begin_connectives() const {
        return connective->begin();
    }
    const_connective_iterator end_connectives() const {
        return connective->end();
    }
    //@}

    /// \name Argument Iterators.
    ///
    /// \brief Iterators over the arguments of an unambiguous call.
    ///
    /// These iterators can be accessed only when this call is unambiguous,
    /// otherwise an assertion will be raised.
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
    /// \brief Iterators over the positional arguments of a call.
    ///
    /// Unlike the more specific arg_iterator, positional iterators are valid
    /// even when the call is ambiguous.
    //@{
    arg_iterator begin_positional() {
        return numPositional ? &arguments[0] : 0;
    }
    arg_iterator end_positional() {
        return numPositional ? &arguments[numPositional] : 0;
    }

    const_arg_iterator begin_positional() const {
        return numPositional ? &arguments[0] : 0;
    }
    const_arg_iterator end_positional() const {
        return numPositional ? &arguments[numPositional] : 0;
    }
    //@}

    /// \name KeywordSelector Iterators.
    ///
    /// \brief Iterators over the keyword selectors of this call.
    ///
    /// These iterators are valid even for ambiguous calls.  They provide the
    /// keyword selectors in the order as they were originally supplied to the
    /// constructor.
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

    //@{
    /// Replaces the expression associated with the given iterator.
    void setArgument(arg_iterator I, Expr *expr);
    void setArgument(key_iterator I, Expr *expr);
    //@}

    /// Converts this SubroutineCall into a raw Ast node.
    Ast *asAst();
    const Ast *asAst() const;

    // Support isa and dyn_cast.
    static bool classof(const Ast *node) {
        Ast::AstKind kind = node->getKind();
        return (kind == Ast::AST_FunctionCallExpr ||
                kind == Ast::AST_ProcedureCallStmt);
    }
    static bool classof(const FunctionCallExpr *node) { return true; }
    static bool classof(const ProcedureCallStmt *node) { return true; }

protected:
    SubroutineRef *connective;
    Expr **arguments;
    KeywordSelector **keyedArgs;
    unsigned numPositional;
    unsigned numKeys;

    /// Returns true if the given declaration is compatable with this kind of
    /// call.
    bool isCompatable(SubroutineDecl *decl) const;

    /// Returns the index of the given expression in the argument expression
    /// array, or -1 if the expression does not exists.
    int argExprIndex(Expr *expr) const;

    /// Returns the index of the given expression in the keyed argument array,
    /// or -1 if the expression does not exists.
    int keyExprIndex(Expr *expr) const;

private:
    /// Helper for the constructors.  Initializes the argument data.
    void initializeArguments(Expr **posArgs, unsigned numPos,
                             KeywordSelector **keyArgs, unsigned numKeys);
};

} // end comma namespace.

namespace llvm {

// Specialize isa_impl_wrap to test if a SubroutineCall is a specific Ast node.
template<class To>
struct isa_impl_wrap<To,
                     const comma::SubroutineCall, const comma::SubroutineCall> {
    static bool doit(const comma::SubroutineCall &val) {
        return To::classof(val.asAst());
    }
};

template<class To>
struct isa_impl_wrap<To, comma::SubroutineCall, comma::SubroutineCall>
  : public isa_impl_wrap<To,
                         const comma::SubroutineCall,
                         const comma::SubroutineCall> { };

// Ast to SubroutineCall conversions.
template<class From>
struct cast_convert_val<comma::SubroutineCall, From, From> {
    static comma::SubroutineCall &doit(const From &val) {
        const From *ptr = &val;
        return (dyn_cast<comma::FunctionCallExpr>(ptr) ||
                dyn_cast<comma::ProcedureCallStmt>(ptr));
    }
};

template<class From>
struct cast_convert_val<comma::SubroutineCall, From*, From*> {
    static comma::SubroutineCall *doit(const From *val) {
        return (dyn_cast<comma::FunctionCallExpr>(val) ||
                dyn_cast<comma::ProcedureCallStmt>(val));
    }
};

template<class From>
struct cast_convert_val<const comma::SubroutineCall, From, From> {
    static const comma::SubroutineCall &doit(const From &val) {
        const From *ptr = &val;
        return (dyn_cast<comma::FunctionCallExpr>(ptr) ||
                dyn_cast<comma::ProcedureCallStmt>(ptr));
    }
};

template<class From>
struct cast_convert_val<const comma::SubroutineCall, From*, From*> {
    static const comma::SubroutineCall *doit(const From *val) {
        return (dyn_cast<comma::FunctionCallExpr>(val) ||
                dyn_cast<comma::ProcedureCallStmt>(val));
    }
};

// SubroutineCall to Ast conversions.
template<class To>
struct cast_convert_val<To,
                        const comma::SubroutineCall,
                        const comma::SubroutineCall> {
    static To &doit(const comma::SubroutineCall &val) {
        return *reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val.asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::SubroutineCall, comma::SubroutineCall>
    : public cast_convert_val<To,
                              const comma::SubroutineCall,
                              const comma::SubroutineCall> { };

template<class To>
struct cast_convert_val<To,
                        const comma::SubroutineCall*,
                        const comma::SubroutineCall*> {
    static To *doit(const comma::SubroutineCall *val) {
        return reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val->asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::SubroutineCall*, comma::SubroutineCall*>
    : public cast_convert_val<To,
                              const comma::SubroutineCall*,
                              const comma::SubroutineCall*> { };

} // end llvm namespace;


#endif
