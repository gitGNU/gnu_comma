//===-- ast/Expr.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_EXPR_HDR_GUARD
#define COMMA_AST_EXPR_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Decl.h"
#include "comma/ast/SubroutineCall.h"
#include "comma/ast/SubroutineRef.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
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

    virtual ~Expr() { }

    /// Returns the location of this expression.
    Location getLocation() const { return location; }

    // Returns true if this expression has a single known type associated with
    // it.
    bool hasType() const { return type != 0; }

    // Most expressions have a single well-known type.  Others, such as
    // FunctionCallExpr, can have multiple types temporarily associated with
    // pending resolution in the type checker.
    //
    // If this expression has a single well-known type, this method returns it.
    // However, if the type is not known, this method will assert.  One can call
    // Expr::hasType to know if a type is available.
    Type *getType() const {
        assert(hasType() && "Expr does not have an associated type!");
        return type;
    }

    // Sets the type of this expression.  If the supplied type is NULL, then
    // this expression is not associated with a type and Expr::hasType will
    // subsequently return false.
    void setType(Type *type) { this->type = type; }

    /// Returns true if this expression can be evaluated as to a static discrete
    /// value.
    bool isStaticDiscreteExpr() const;

    /// Attempts to evaluate this expression as a constant descrete expression.
    ///
    /// The correctness of this evaluation depends on the correctness of the
    /// given expression.  For example, no checks are made that two distinct
    /// types are summed together.
    ///
    /// \param result If this is a static expression, \p result is set to the
    /// computed value.
    ///
    /// \return True if \p expr is static and \p result was set.  False otherwise.
    bool staticDiscreteValue(llvm::APInt &result) const;

    /// Attempts to evaluate this expression as a constant string expression.
    ///
    /// \param result If this is a static expression, \p result is set to the
    /// computed value.
    ///
    /// \return True if \p expr is static and \p result was set.  False
    /// otherwise.
    bool staticStringValue(std::string &result) const;

    /// Returns true if this expression evaluates to a static string expression.
    bool isStaticStringExpr() const;

    static bool classof(const Expr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesExpr();
    }

private:
    Type *type;
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

class FunctionCallExpr : public Expr, public SubroutineCall {

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

    /// Create a nullary function call expression using the given SubroutineRef
    /// as connective.
    FunctionCallExpr(SubroutineRef *connective);

    /// Create a nullary function call expression using the given FunctionDecl
    /// as connective.
    FunctionCallExpr(FunctionDecl *connective, Location loc);

    /// Returns the location of this function call.
    ///
    /// Implementation required by SubroutineCall.
    Location getLocation() const { return Expr::getLocation(); }

    //@{
    /// If this call is unambiguous, returns the unique function declaration
    /// serving as the connective for this call.
    FunctionDecl *getConnective() {
        return llvm::cast<FunctionDecl>(SubroutineCall::getConnective());
    };

    const FunctionDecl *getConnective() const {
        return llvm::cast<FunctionDecl>(SubroutineCall::getConnective());
    }
    //@}

    /// Resolved the connective for this call.
    void resolveConnective(FunctionDecl *connective);

    //@{
    /// Returns the \p i'th connective associated with this call.  The returned
    /// node is either an EnumLiteral or a regular FunctionDecl.
    const FunctionDecl *getConnective(unsigned i) const {
        return llvm::cast<FunctionDecl>(SubroutineCall::getConnective(i));
    }
    FunctionDecl *getConnective(unsigned i) {
        return llvm::cast<FunctionDecl>(SubroutineCall::getConnective(i));
    }
    //@}

    //@{
    /// Iterators over the set of function declarations serving as connectives
    /// for this function call expression.
    typedef SubroutineRef::fun_iterator fun_iterator;
    fun_iterator begin_functions() {
        return connective->begin_functions();
    }
    fun_iterator end_functions() {
        return connective->end_functions();
    }

    typedef SubroutineRef::const_fun_iterator const_fun_iterator;
    const_fun_iterator begin_functions() const {
        return connective->begin_functions();
    }
    const_fun_iterator end_functions() const {
        return connective->end_functions();
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const FunctionCallExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionCallExpr;
    }

private:
    void setTypeForConnective();
};

//===----------------------------------------------------------------------===//
// IndexedArrayExpr
//
// Represents the indexing into an array expression.
class IndexedArrayExpr : public Expr {

public:
    IndexedArrayExpr(Expr *arrExpr, Expr **indices, unsigned numIndices);

    ///@{
    /// Returns the expression denoting the array to index.
    Expr *getArrayExpr() { return indexedArray; }
    const Expr *getArrayExpr() const { return indexedArray; }
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
    Expr *indexedArray;
    unsigned numIndices;
    Expr **indexExprs;
};

//===----------------------------------------------------------------------===//
// SelectedExpr
//
/// Represents a selected component.
class SelectedExpr : public Expr {

public:
    /// Constructs a selected component of the form <tt>P.C</tt>, where \p
    /// prefix is the expression corresponding to \c P, \p component is the
    /// selected declaration corresponding to \c C, and \p loc is the location
    /// of \p component.
    SelectedExpr(Expr *prefix, Decl *component, Location loc, Type *type)
        : Expr(AST_SelectedExpr, type, prefix->getLocation()),
          prefix(prefix), component(component), componentLoc(loc) { }

    //@{
    /// Returns the prefix of this SelectedExpr.
    const Expr *getPrefix() const { return prefix; }
    Expr *getPrefix() { return prefix; }
    //@}

    //@{
    /// Returns the selected declaration.
    const Decl *getSelector() const { return component; }
    Decl *getSelector() { return component; }
    //@}

    /// Returns the location of the selected declaration.
    Location getSelectorLoc() const { return componentLoc; }

    // Support isa/dyn_cast.
    static bool classof(const SelectedExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SelectedExpr;
    }

private:
    Expr *prefix;
    Decl *component;
    Location componentLoc;
};

//===----------------------------------------------------------------------===//
// InjExpr
//
// Represents "inj" expressions, mapping domain types to their carrier types.
class InjExpr : public Expr {

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
class PrjExpr : public Expr {

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

    IntegerLiteral(const llvm::APInt &value, Type *type, Location loc)
        : Expr(AST_IntegerLiteral, type, loc), value(value) { }

    const llvm::APInt &getValue() const { return value; }
    llvm::APInt &getValue() { return value; }

    void setValue(const llvm::APInt &V) { value = V; }

    static bool classof(const IntegerLiteral *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerLiteral;
    }

private:
    llvm::APInt value;
};

//===----------------------------------------------------------------------===//
// StringLiteral
//
// Initially, strings are constructed without a known type, as they are
// overloaded objects.
class StringLiteral : public Expr
{
    typedef llvm::SmallPtrSet<EnumerationDecl*, 4> InterpSet;

public:
    /// Constructs a string literal over the given raw character data.  The data
    /// is copied by the constructor (it is safe for the data to live in a
    /// shared memory region).
    StringLiteral(const char *string, unsigned len, Location loc)
        : Expr(AST_StringLiteral, loc) {
        init(string, len);
    }

    /// Constructs a string literal using a pair of iterators.
    StringLiteral(const char *start, const char *end, Location loc)
        : Expr(AST_StringLiteral, loc) {
        init(start, end - start);
    }

    /// Returns the type of this string literal once resolved.
    ArrayType *getType() const {
        return llvm::cast<ArrayType>(Expr::getType());
    }

    /// Returns the underlying representation of this string literal.
    llvm::StringRef getString() const { return llvm::StringRef(rep, len); }

    /// Returns the length in characters of this string literal.
    unsigned length() const { return len; }

    /// Adds an interpretation for the component type of this string.
    ///
    /// \param decl The enumeration decl defining a possible component type.
    ///
    /// \return True if the given declaration was a new interpretation and
    /// false if it was already present.
    bool addComponentType(EnumerationDecl *decl) {
        return interps.insert(decl);
    }

    /// Adds a set of interpretations for the component type of this string.
    template <class I>
    void addComponentTypes(I start, I end) { interps.insert(start, end); }

    /// Removes an interpretation for the component type of this string.
    ///
    /// \param decl The enumeration decl to remove.
    ///
    /// \return True if the given enumeration was removed from the set of
    /// interpretations, and false if it did not exist.
    bool removeComponentType(EnumerationDecl *decl) {
        return interps.erase(decl);
    }

    /// Tests if the given enumeration type or declaration is in the set of
    /// component types.
    ///
    /// \param decl The enumeration decl or type to test.
    ///
    /// \return True if the given declaration is registered as a component type
    /// and false otherwise.
    //@{
    bool containsComponentType(EnumerationDecl *decl) const {
        return interps.count(decl);
    }
    bool containsComponentType(EnumerationType *type) const {
        return findComponent(type->getRootType()) != end_component_types();
    }
    //@}

    /// Sets the component type of this literal to the given type, dropping all
    /// other interpretations, and returns true.  If the given type does not
    /// describe an interpretation of this literal nothing is done and false is
    /// returned.
    //@{
    bool resolveComponentType(EnumerationType *type);
    //@}

    /// Returns the number of component interpretations.
    unsigned numComponentTypes() const { return interps.size(); }

    /// Returns true if the set of component types is empty.
    bool zeroComponentTypes() const { return numComponentTypes() == 0; }

    /// If there is a unique component type, return it, else null.
    const EnumerationDecl *getComponentType() const {
        if (numComponentTypes() != 1)
            return 0;
        return *begin_component_types();
    }


    //@{
    /// Iterators over the set of component types.
    typedef InterpSet::iterator component_iterator;
    component_iterator begin_component_types() { return interps.begin(); }
    component_iterator end_component_types() { return interps.end(); }

    typedef InterpSet::const_iterator const_component_iterator;
    const_component_iterator begin_component_types() const {
        return interps.begin();
    }
    const_component_iterator end_component_types() const {
        return interps.end();
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const StringLiteral *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_StringLiteral;
    }

private:
    char *rep;
    unsigned len;
    InterpSet interps;

    /// Initialize this literal with the given character data.
    void init(const char *string, unsigned len);

    /// Returns an iterator to the given component declaration with the given
    /// type.
    const_component_iterator findComponent(EnumerationType *type) const;
    component_iterator findComponent(EnumerationType *type);
};

//===----------------------------------------------------------------------===//
// NullExpr
//
/// \class
///
/// \brief Represents a null value of some access type.
///
/// Null expressions are fairly similar to integer literals and aggregate
/// expressions in the sense that they do not, initially, have a type.  Nodes of
/// this kind are, generally, typeless when first constructed.  It is only after
/// a context as been resolved for the null expression a final type can be
/// assigned.
class NullExpr : public Expr {

public:
    NullExpr(Location loc, AccessType *target = 0)
        : Expr(AST_NullExpr, target, loc) { }

    //@{
    /// Specialize Expr::getType().
    const AccessType *getType() const {
        return llvm::cast<AccessType>(Expr::getType());
    }
    AccessType *getType() {
        return llvm::cast<AccessType>(Expr::getType());
    }
    //@}

    // Support isa/dyn_cast;
    static bool classof(const NullExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_NullExpr;
    }
};

//===----------------------------------------------------------------------===//
// QualifiedExpr
//
/// \class
///
/// \brief Nodes representing qualified expressions.
class QualifiedExpr : public Expr {

public:
    QualifiedExpr(TypeDecl *qualifier, Expr *operand, Location loc)
        : Expr(AST_QualifiedExpr, qualifier->getType(), loc),
          prefix(qualifier), operand(operand) { }

    ~QualifiedExpr() { delete operand; }

    //@{
    /// Returns the type declaration which qualifies this expression.
    const TypeDecl *getPrefix() const { return prefix; }
    TypeDecl *getPrefix() { return prefix; }
    //@}

    //@{
    /// Returns the expression this node qualifies.
    const Expr *getOperand() const { return operand; }
    Expr *getOperand() { return operand; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const QualifiedExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_QualifiedExpr;
    }

private:
    TypeDecl *prefix;
    Expr *operand;
};

//===----------------------------------------------------------------------===//
// ConversionExpr
//
// These nodes represent type conversions.
class ConversionExpr : public Expr
{
public:
    ConversionExpr(Expr *operand, Type *target, Location loc = 0)
        : Expr(AST_ConversionExpr, target, loc),
          operand(operand) { }

    /// Returns the expression to be converted.
    const Expr *getOperand() const { return operand; }
    Expr *getOperand() { return operand; }

    // Support isa and dyn_cast.
    static bool classof(const ConversionExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ConversionExpr;
    }

private:
    Expr *operand;              ///< The expression to convert.
};

} // End comma namespace.

#endif
