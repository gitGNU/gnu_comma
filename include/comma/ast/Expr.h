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

namespace comma {

//===----------------------------------------------------------------------===//
// Expr
//
/// \class
///
/// \brief This is the root of the AST hierarchy representing expressions.
class Expr : public Ast {

public:
    Expr(AstKind kind, Type *type, Location loc = Location())
        : Ast(kind), type(type), location(loc) {
        assert(this->denotesExpr());
    }

    Expr(AstKind kind, Location loc = Location())
        : Ast(kind), type(0), location(loc) {
        assert(this->denotesExpr());
    }

    virtual ~Expr() { }

    /// Returns the location of this expression.
    Location getLocation() const { return location; }

    /// Returns true if this expression has a type associated with it.
    bool hasType() const { return type != 0; }

    /// \brief Returns the type of this expression.
    ///
    /// Most expressions have a single well-known type.  Others, such as
    /// FunctionCallExpr, can have multiple types temporarily associated with
    /// pending resolution in the type checker.
    ///
    /// If this expression has a single well-known type, this method returns it.
    /// However, if the type is not known, this method will assert.  One can
    /// call Expr::hasType to know if a type is available.
    Type *getType() const {
        assert(hasType() && "Expr does not have an associated type!");
        return type;
    }

    /// \brief Sets the type of this expression.
    ///
    /// If the supplied type is null, then this expression is not associated
    /// with a type and Expr::hasType will subsequently return false.
    void setType(Type *type) { this->type = type; }

    /// Returns true if this expression has a resolved type.
    ///
    /// An expression is considered to have a resolved type if its type has been
    /// set and it is not a universal type.
    bool hasResolvedType() const {
        return hasType() && !getType()->isUniversalType();
    }

    /// \brief Returns true if this expression can be evaluated as to a static
    /// discrete value.
    bool isStaticDiscreteExpr() const;

    /// \brief Attempts to evaluate this expression as a constant descrete
    /// expression.
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

    /// \brief Attempts to evaluate this expression as a constant string
    /// expression.
    ///
    /// \param result If this is a static expression, \p result is set to the
    /// computed value.
    ///
    /// \return True if \p expr is static and \p result was set.  False
    /// otherwise.
    bool staticStringValue(std::string &result) const;

    /// \brief Returns true if this expression evaluates to a static string
    /// expression.
    bool isStaticStringExpr() const;

    /// \brief Determines if this expression is mutable.
    ///
    /// Mutable expressions always reduce to a DeclRefExpr which denotes:
    ///
    ///   - An object declaration;
    ///
    ///   - A ParamValueDecl of mode "out" or "in out";
    ///
    ///   - A renamed object declaration which signifies a mutable expression.
    ///
    /// A mutable DeclRefExpr may be wrapped by a dereference, component
    /// selection, or array index expression.
    ///
    /// \param immutable If \c this is not a mutable expression then \p
    /// immutable is set to first subexpression which forbids mutability.  The
    /// purpose of this parameter is to provide the caller with the context as
    /// to why \c this is not mutable.
    ///
    /// \return True if this is a mutable expression false otherwise.
    bool isMutable(Expr *&immutable);

    /// Returns true if this node denotes a name as defined in the Comma
    /// grammar.
    bool denotesName() const;

    /// Returns a copy of this Expr.
    ///
    /// FIXME: Implement.
    Expr *clone() { return this; }

    // Support isa/dyn_cast.
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

    /// Returns the defining identifier of the declaration this expression
    /// references.
    IdentifierInfo *getIdInfo() const { return declaration->getIdInfo(); }

    /// Returns a string representation of the defining identifier.
    const char *getString() const { return declaration->getString(); }

    //@{
    /// Returns the declaration this expression references.
    const ValueDecl *getDeclaration() const { return declaration; }
    ValueDecl *getDeclaration() { return declaration; }
    //@}
    void setDeclaration(ValueDecl *decl) { declaration = decl; }

    // Support isa/dyn_cast.
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

    /// Creates a resolved function call expression over the given function
    /// declaration.
    FunctionCallExpr(FunctionDecl *connective, Location loc,
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
    Expr *getPrefix() { return indexedArray; }
    const Expr *getPrefix() const { return indexedArray; }
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

    /// Constructs an ambiguous selected component.  SelectedExpr nodes produced
    /// by this constructor must be filled in with a call to resolve().
    SelectedExpr(Expr *prefix, IdentifierInfo *component, Location loc)
        : Expr(AST_SelectedExpr, prefix->getLocation()),
          prefix(prefix), component(component), componentLoc(loc) { }

    /// Returns true if this selected expression is ambiguous.
    bool isAmbiguous() const { return component.is<IdentifierInfo*>(); }

    /// Resolves this selected expression.
    void resolve(Decl *component, Type *type) {
        assert(isAmbiguous() && "SelectedExpr already resolved!");
        this->component = component;
        setType(type);
    }

    //@{
    /// Returns the prefix of this SelectedExpr.
    const Expr *getPrefix() const { return prefix; }
    Expr *getPrefix() { return prefix; }
    //@}

    //@{
    /// Returns the selected declaration.  If this is an ambiguous selected
    /// expression an assertion will fire.
    const Decl *getSelectorDecl() const { return component.get<Decl*>(); }
    Decl *getSelectorDecl() { return component.get<Decl*>(); }
    //@}

    /// Returns the identifier associated with this selected expression.
    IdentifierInfo *getSelectorIdInfo() const {
        if (component.is<IdentifierInfo*>())
            return component.get<IdentifierInfo*>();
        else
            return component.get<Decl*>()->getIdInfo();
    }

    /// Returns the location of the selected declaration.
    Location getSelectorLoc() const { return componentLoc; }

    // Support isa/dyn_cast.
    static bool classof(const SelectedExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SelectedExpr;
    }

private:
    typedef llvm::PointerUnion<Decl*, IdentifierInfo*> ComponentUnion;

    Expr *prefix;
    ComponentUnion component;
    Location componentLoc;
};

//===----------------------------------------------------------------------===//
// IntegerLiteral
//
class IntegerLiteral : public Expr
{
public:
    /// Constructs an integer literal with an initial type of \c
    /// universal_integer.
    IntegerLiteral(const llvm::APInt &value, Location loc)
        : Expr(AST_IntegerLiteral, UniversalType::getUniversalInteger(), loc),
          value(value) { }

    /// Constructs an integer literal with the given integer type.
    IntegerLiteral(const llvm::APInt &value, IntegerType *type, Location loc)
        : Expr(AST_IntegerLiteral, type, loc), value(value) { }

    /// Returns true if this literal is of the universal integer type.
    bool isUniversalInteger() const {
        return llvm::isa<UniversalType>(getType());
    }

    //@{
    /// Returns the literal value of this integer.
    const llvm::APInt &getValue() const { return value; }
    llvm::APInt &getValue() { return value; }
    //@}

    /// Sets the literal value of this integer.
    void setValue(const llvm::APInt &V) { value = V; }

    // Suppport isa/dyn_cast.
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
// DereferenceExpr
//
/// \class
///
/// \brief Representes either an explicit or implicit dereference expression.
class DereferenceExpr : public Expr {

public:
    DereferenceExpr(Expr *prefix, Location loc, bool isImplicit = false);

    ~DereferenceExpr() { delete prefix; }

    //@{
    /// Returns the expression to which this dereference applies.
    const Expr *getPrefix() const { return prefix; }
    Expr *getPrefix() { return prefix; }
    //@}

    //@{
    /// Convenience method returning the type of the prefix.
    const AccessType *getPrefixType() const {
        return llvm::cast<AccessType>(prefix->getType());
    }
    AccessType *getPrefixType() {
        return llvm::cast<AccessType>(prefix->getType());
    }
    //@}

    /// Returns true if this dereference was implicitly generated by the
    /// compiler.
    bool isImplicit() const { return bits != 0; }

    // Support isa/dyn_cast.
    static bool classof(const DereferenceExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DereferenceExpr;
    }

private:
    Expr *prefix;
};

//===----------------------------------------------------------------------===//
// ConversionExpr
//
// These nodes represent type conversions.
class ConversionExpr : public Expr
{
public:
    ConversionExpr(Expr *operand, Type *target, Location loc = Location())
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

//===----------------------------------------------------------------------===//
// AllocatorExpr
//
/// \class
///
/// \brief Representation of allocation expressions.
///
/// There are two types of allocation expressions -- initialized and
/// uninitialized.  This class represents both simultaneously.  The approach
/// taken is to view an initialized allocation as a special case of an
/// uninitialized allocation.
///
/// Similar to NullExpr's, AllocatorExpr's are initially constructed without a
/// type.  The expected type of an allocation expression is inferred from the
/// context in which it appears.
class AllocatorExpr : public Expr {

public:
    /// Creates an initialized allocation over the given qualified expression.
    AllocatorExpr(QualifiedExpr *operand, Location loc)
        : Expr(AST_AllocatorExpr, loc),
          operand(operand) { }

    /// Creates an uninitialized allocation over the given primary type.
    AllocatorExpr(PrimaryType *operand, Location loc)
        : Expr(AST_AllocatorExpr, loc),
          operand(operand) { }

    /// Returns true if this is an initialized allocation expression.
    bool isInitialized() const { return operand.is<Expr*>(); }

    /// Returns true if this is an uninitialized allocation expression.
    bool isUninitialized() const { return operand.is<PrimaryType*>(); }

    //@{
    /// Returns the initializer associated with this allocator or null if this
    /// is not an initialized allocator.
    const Expr *getInitializer() const { return operand.dyn_cast<Expr*>(); }
    Expr *getInitializer() { return operand.dyn_cast<Expr*>(); }
    //@}

    //@{
    /// Returns the type this allocator allocates.
    const PrimaryType *getAllocatedType() const {
        return const_cast<AllocatorExpr*>(this)->getAllocatedType();
    }
    PrimaryType *getAllocatedType() {
        if (isInitialized())
            return llvm::cast<PrimaryType>(operand.get<Expr*>()->getType());
        else
            return operand.get<PrimaryType*>();
    }
    //@}

    /// Sets the initializer of this allocator expression.
    void setInitializer(Expr *expr) { operand = expr; }

    //@{
    /// Specialize Expr::getType().
    const AccessType *getType() const {
        return llvm::cast<AccessType>(Expr::getType());
    }
    AccessType *getType() {
        return llvm::cast<AccessType>(Expr::getType());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const AllocatorExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AllocatorExpr;
    }

private:
    /// Underlying representation is a discriminated union of either a primary
    /// type or an expression.
    typedef llvm::PointerUnion<PrimaryType*, Expr*> OperandUnion;
    OperandUnion operand;
};

//===----------------------------------------------------------------------===//
// DiamondExpr
//
/// \class
///
/// A DiamondExpr is a very simple node which correspond to the occurrence of an
/// "<>" token in an expression.  Such expressions denote the default value
/// corresponding to the associated type.
class DiamondExpr : public Expr {

public:
    DiamondExpr(Location loc, Type *type = 0)
        : Expr(AST_DiamondExpr, type, loc) { }

    // Support isa/dyn_cast.
    static bool classof(const DiamondExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DiamondExpr;
    }
};

} // End comma namespace.

#endif
