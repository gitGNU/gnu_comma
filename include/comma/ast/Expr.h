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

//===----------------------------------------------------------------------===//
// AggregateExpr
//
/// A common base class for all aggregate expressions.
class AggregateExpr : public Expr {

public:
    virtual ~AggregateExpr() { delete othersComponent; }

    /// An enumeration defining the kind of "others" component associated with
    /// this aggregate.
    enum OthersKind {
        Others_None,            //< There is no \c others component.
        Others_Undef,           //< Indicates a <tt>others => <></tt> component.
        Others_Expr             //< An expression has been given for \c others.
    };

    /// Returns the kind of \c others component associated with this aggregate.
    OthersKind getOthersKind() const { return static_cast<OthersKind>(bits); }

    /// Returns true if this aggregate has a \c others component.
    bool hasOthers() const { return getOthersKind() != Others_None; }

    /// Returns true if this aggregate is empty.
    ///
    /// An aggregate is empty if there are no components associated with the
    /// expression and no others clause.  Aggregate expression nodes should
    /// never be empty in a well-formed Comma program.
    virtual bool empty() const = 0;

    //@{
    /// Returns the expression associated with a \c others component.
    ///
    /// \returns If hasOthers() returns \c Others_Expr then this method returns
    /// the associated expression.  Otherwise, this method returns null.
    Expr *getOthersExpr() { return othersComponent; }
    const Expr *getOthersExpr() const { return othersComponent; }
    //@}

    /// \brief Returns the location of the \c others reserved word, or an
    /// invalid location if hasOthers() returns false.
    Location getOthersLoc() const { return othersLoc; }

    /// Associates an expression with the \c others component of this aggregate.
    ///
    /// Aggregates cannot hold multiple \c others components.  This method will
    /// assert if called more than once.
    ///
    /// \param loc Location of the \c others reserved word.
    ///
    /// \param component The expression associated with \c others.
    void addOthersExpr(Location loc, Expr *component) {
        assert(!hasOthers() && "Others component already set!");
        othersLoc = loc;
        othersComponent = component;
        bits = Others_Expr;
    }

    /// Associates an undefined \c others component with this aggregate.
    ///
    /// \param loc Location of the \c others reserved word.
    void addOthersUndef(Location loc) {
        assert(!hasOthers() && "Others component already set!");
        othersLoc = loc;
        bits = Others_Undef;
    }

    /// Replaces an existing others expression.
    void setOthersExpr(Expr *expr) { othersComponent = expr; }

    // Support isa/dyn_cast.
    static bool classof(const AggregateExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return denotesAggregateExpr(node->getKind());
    }

protected:
    AggregateExpr(AstKind kind, Location loc)
        : Expr(kind, loc), othersLoc(0), othersComponent(0) {
        assert(denotesAggregateExpr(kind));
        bits = Others_None;
    }

private:
    static bool denotesAggregateExpr(AstKind kind) {
        return (kind == AST_PositionalAggExpr || kind == AST_KeyedAggExpr);
    }

    // Location of the "others" reserved word.
    Location othersLoc;

    // Expression associated with an "others" component.
    Expr *othersComponent;
};

//===----------------------------------------------------------------------===//
// PositionalAggExpr
//
/// Ast node representing both record and array aggregate expressions with
/// components provided positionally.
class PositionalAggExpr : public AggregateExpr {

    /// Type used to store the component expressions.
    typedef std::vector<Expr*> ComponentVec;

public:
    PositionalAggExpr(Location loc)
        : AggregateExpr(AST_PositionalAggExpr, loc) { }

    template<class Iter>
    PositionalAggExpr(Iter I, Iter E, Location loc)
        : AggregateExpr(AST_PositionalAggExpr, loc),
          components(I, E) { }

    /// Returns the number of components in this aggregate.
    unsigned numComponents() const { return components.size(); }

    /// Adds a component to this aggregate expression.
    ///
    /// The order in which this method is called determines the order of the
    /// components.
    void addComponent(Expr *expr) { components.push_back(expr); }

    /// \name Component Iterators.
    //@{
    typedef ComponentVec::iterator iterator;
    iterator begin_components() { return components.begin(); }
    iterator end_components() { return components.end(); }

    typedef ComponentVec::const_iterator const_iterator;
    const_iterator begin_components() const { return components.begin(); }
    const_iterator end_components() const { return components.end(); }
    //@}

    /// \brief Returns true if this aggregate expression has not been populated
    /// with any components.
    ///
    /// Note that this predicate will return false if an "others" component is
    /// present.
    bool empty() const {
        return (numComponents() == 0) && (getOthersKind() != Others_None);
    }

    // Support isa and dyn_cast.
    static bool classof(const PositionalAggExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PositionalAggExpr;
    }

private:
    // Vector of expressions forming the components of this aggregate
    // expression.
    std::vector<Expr*> components;
};

//===----------------------------------------------------------------------===//
// KeyedAggExpr
//
/// A KeyedAggExpr represents aggregate expressions where the components are
/// specified using discrete choice lists.
class KeyedAggExpr : public AggregateExpr {

public:
    /// Constructs and empty Keyed aggregate expression.
    ///
    /// \param loc Location of the opening parenthesis for this aggregate.
    KeyedAggExpr(Location loc) :
        AggregateExpr(AST_KeyedAggExpr, loc) { }

    ~KeyedAggExpr();

    /// \class
    ///
    /// The following class represents a discrete choice list and the associated
    /// expression.
    class ChoiceList {

        /// KeyedAggExpr is the sole class permited to construct and destruct
        /// ChoiceList objects.  The following private part defines the
        /// interface which KeyedAggExpr is expected to limit itself to.
        friend class KeyedAggExpr;

        /// Constructs a ChoiceList over the given set of discrete choices.
        /// This factory method is intended to be used by KeyedAggExpr
        /// exclusively.  Each discrete choice must yield an ast node which is
        /// either:
        ///
        ///   - a range;
        ///
        ///   - an expression;
        ///
        ///   - or a discrete subtype declaration.
        ///
        /// In addition, there must be at least one choice provided.
        static ChoiceList *create(Ast **choices, unsigned numChoices,
                                  Expr *expr);

        /// Deallocates a ChoiceList.
        static void dispose(ChoiceList *CL);

    public:
        //@{
        /// Returns the expression associated with this ChoiceList.
        const Expr *getExpr() const { return expr; }
        Expr *getExpr() { return expr; }
        //@}

        /// Sets the expression associated with this ChoiceList.
        void setExpr(Expr *expr) { this->expr = expr; }

        /// Returns the number of choices associated with this ChoiceList.
        unsigned numChoices() const { return choiceEntries; }

        /// \name Choice Accessors by Index.
        ///
        /// Returns the i'th choice associated with this list, using
        /// llvm::dyn_cast to resolve the type.  In other words, if the choice
        /// at the given index is not of the supplied type, this method returns
        /// null.  Usage example:
        ///
        /// \code
        ///   if (Range *range = CL.getChoice<Range>(i)) { ... }
        /// \endcode
        //@{
        template <class T>
        T *getChoice(unsigned i) {
            assert(i < choiceEntries && "Index out of range!");
            return llvm::dyn_cast<T>(choiceData[i]);
        }

        template <class T>
        const T *getChoice(unsigned i) const {
            assert(i < choiceEntries && "Index out of range!");
            return llvm::dyn_cast<T>(choiceData[i]);
        }
        //@}

        //@{
        /// \name Choice Iterators.
        ///
        /// \brief Iterators over each choice provided by this ChoiceList.
        typedef Ast **iterator;
        iterator begin() { return choiceData; }
        iterator end() { return &choiceData[choiceEntries]; }

        typedef Ast *const *const_iterator;
        const_iterator begin() const { return choiceData; }
        const_iterator end() const { return &choiceData[choiceEntries]; }
        //@}

        // Support isa/dyn_cast.
        static bool classof(const KeyedAggExpr *node) { return true; }
        static bool classof(const Ast *node) {
            return node->getKind() == AST_KeyedAggExpr;
        }

    private:
        /// Constructor for use by ChoiceList::create.
        ChoiceList(Ast **choices, unsigned choiceEntries, Expr *expr);

        ChoiceList(const ChoiceList &CL);             // Do not implement.
        ChoiceList &operator =(const ChoiceList &CL); // Likewise.

        /// Internally, ChoiceList's are allocated with the actual ast
        /// expressions allocated immediately after the node itself.  Hense
        /// choiceData points to address <tt>this + 1</tt>.
        Ast **choiceData;
        unsigned choiceEntries;
        Expr *expr;
    };

    /// Adds a discrete choice list to this KeyedAggExpr.  Each element of \p
    /// choices corresponds to a particular discrete choice, each of which must
    /// yield an ast node that is either:
    ///
    ///   - a range;
    ///
    ///   - an expression;
    ///
    ///   - or a discrete subtype declaration.
    ///
    /// In addition, there must be at least one choice provided.
    void addDiscreteChoice(Ast **choices, unsigned numChoices, Expr *expr) {
        this->choices.push_back(ChoiceList::create(choices, numChoices, expr));
    }

    /// Returns the number of dicrete choice lists provided by this aggregate.
    unsigned numDiscreteChoiceLists() const { return choices.size(); }

    /// Returns the total number of choices provided by this aggregate.
    ///
    /// \note This method scans the entire aggregate and recomputes the number
    /// of choices on each call.  Use sparingly as aggregates can potentially be
    /// quite large.
    unsigned numChoices() const;

    /// Returns true if this aggregate is empty, meaning there are no components
    /// defined and no others clause.
    bool empty() const { return numDiscreteChoiceLists() == 0 && !hasOthers(); }

    /// \name ChoiceList Iterators.
    ///
    /// \brief Iterators over the choice lists associated with this aggregate.
    //@{
    typedef std::vector<ChoiceList*>::iterator cl_iterator;
    cl_iterator cl_begin() { return choices.begin(); }
    cl_iterator cl_end() { return choices.end(); }

    typedef std::vector<ChoiceList*>::const_iterator const_cl_iterator;
    const_cl_iterator cl_begin() const { return choices.begin(); }
    const_cl_iterator cl_end() const { return choices.end(); }
    //@}

private:
    /// \class
    ///
    /// The following class provides a simple forward iterator over each
    /// discrete choice provided by an aggregate expression.
    class ChoiceIterator {
        std::vector<ChoiceList*> &choices;
        unsigned choiceIdx;     // Index into the choices vector.
        unsigned listIdx;       // Index into the corresponding ChoiceList.

        /// Internal constructor used for the generation of sentinel iterators.
        ChoiceIterator(std::vector<ChoiceList*> &choices,
                       unsigned choiceIdx, unsigned listIdx)
            : choices(choices), choiceIdx(choiceIdx), listIdx(listIdx) { }

        /// Constructor for use by KeyedAggExpr.
        ChoiceIterator(std::vector<ChoiceList*> &choices)
            : choices(choices), choiceIdx(0), listIdx(0) { }

        /// Sentinel producing method for use by KeyedAggExpr.
        static ChoiceIterator getSentinel(std::vector<ChoiceList*> &choices) {
            return ChoiceIterator(choices, choices.size(), 0);
        }

        friend class KeyedAggExpr;

    public:

        bool operator ==(const ChoiceIterator &iter) const {
            return choiceIdx == iter.choiceIdx && listIdx == iter.listIdx;
        }

        bool operator !=(const ChoiceIterator &iter) const {
            return !this->operator==(iter);
        }

        ChoiceIterator &operator++() {
            listIdx = listIdx + 1;
            if (listIdx == choices[choiceIdx]->numChoices()) {
                choiceIdx = choiceIdx + 1;
                listIdx = 0;
            }
            return *this;
        }

        ChoiceIterator operator++(int) {
            ChoiceIterator res = *this;
            this->operator++();
            return res;
        }

        Ast *operator*() {
            return choices[choiceIdx]->getChoice<Ast>(listIdx);
        }

        Expr *getExpr() { return choices[choiceIdx]->getExpr(); }
    };

public:
    /// \name Choice Iterators.
    ///
    /// \brief Iterators over each discrete choice associated with this
    /// aggregate.
    ///
    /// These iterators traverse the set of choices in the order they were added
    /// to the aggregate node.
    //@{
    typedef ChoiceIterator choice_iterator;
    choice_iterator choice_begin() { return ChoiceIterator(choices); }
    choice_iterator choice_end() {
        return ChoiceIterator::getSentinel(choices);
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const KeyedAggExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_KeyedAggExpr;
    }

private:
    std::vector<ChoiceList*> choices;
};

} // End comma namespace.

#endif
