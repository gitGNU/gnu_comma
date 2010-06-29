//===-- ast/AggExpr.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_AGGEXPR_HDR_GUARD
#define COMMA_AST_AGGEXPR_HDR_GUARD

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Provides AST node definitions for the representation of aggregate
/// expressions.
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/Type.h"
#include "comma/ast/TypeRef.h"

namespace comma {

//===----------------------------------------------------------------------===//
// Identifier
//
/// \class
///
/// \brief A simple class wrapping an identifierInfo together with a location.
///
/// This node is used as a placeholder for identifiers that appear as keys in
/// aggregate expressions.  Processing of an aggregate expression is, in
/// general, defered until a type context can be resolved.  Since there is no
/// syntatic way to distinguish a record selector from any other name that might
/// be in scope, we need a class to represent this special case in the AST.  As
/// an example of the issue consider the following:
///
/// \code
/// type E is (X, Y);
///
/// type A is array (E) of Integer;
///
/// type R is record
///    X : Integer;
///    Y : Integer;
/// end record;
///
/// Foo : A := (X => 1, Y => 1);
/// Bar : R := (X => 1, Y => 1);
/// \endcode
///
/// There is no way to assign an exact representation for X or Y in the
/// aggregate expressions until the expected type of the aggregate is known.
class Identifier : public Ast {

public:
    Identifier(IdentifierInfo *name, Location loc)
        : Ast(AST_Identifier), name(name), loc(loc) { }

    /// Returns the IdentifierInfo of this Identifier.
    IdentifierInfo *getIdInfo() const { return name; }

    /// Returns the Location of this Identifier.
    Location getLocation() const { return loc; }

    /// Sets the associated IdentifierInfo.
    void setIdInfo(IdentifierInfo *name) { this->name = name; }

    /// Sets the associated Location.
    void setLocation(Location loc) { this->loc = loc; }

    // Support isa/dyn_cast;
    static bool classof(const Identifier *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_Identifier;
    }

private:
    IdentifierInfo *name;
    Location loc;
};

//===----------------------------------------------------------------------===//
// ComponentKey
//
/// \class
///
/// \brief A union of various AST nodes that can be used as the key in an
/// aggregate expression.
///
/// This class provides a checked union to represent the nodes which can appear
/// in a discrete choice, plus Identifier nodes as needed by record aggregates.
class ComponentKey : public Ast {

public:
    /// \name Constructors.
    //@{
    ComponentKey(Expr *node)
        : Ast(AST_ComponentKey), rep(node), loc(node->getLocation()) { }
    ComponentKey(Identifier *node)
        : Ast(AST_ComponentKey), rep(node), loc(node->getLocation()) { }
    ComponentKey(Range *node)
        : Ast(AST_ComponentKey), rep(node), loc(node->getLowerLocation()) { }
    ComponentKey(TypeRef *node)
        : Ast(AST_ComponentKey), rep(node), loc(node->getLocation()) { }
    ComponentKey(ComponentDecl *node, Location loc)
        : Ast(AST_ComponentKey), rep(node), loc(loc) { }
    //@}

    /// \name Representation Predicates.
    //@{
    bool denotesExpr()       const { return llvm::isa<Expr>(rep);           }
    bool denotesIdentifier() const { return llvm::isa<Identifier>(rep);     }
    bool denotesRange()      const { return llvm::isa<Range>(rep);          }
    bool denotesType()       const { return llvm::isa<TypeRef>(rep);        }
    bool denotesComponent()  const { return llvm::isa<ComponentDecl>(rep) ; }
    //@}

    /// Returns the location of this ComponentKey.
    Location getLocation() const { return loc; }

    /// Sets the location of this ComponentKey.
    void setLocation(Location loc) { this->loc = loc; }

    /// \name Representation Getters.
    //@{
    /// Returns the underlying node if it is of the corresponding type else
    /// null.
    Expr *getAsExpr() {
        return llvm::dyn_cast<Expr>(rep);
    }
    Identifier *getAsIdentifier() {
        return llvm::dyn_cast<Identifier>(rep);
    }
    Range *getAsRange() {
        return llvm::dyn_cast<Range>(rep);
    }
    TypeRef *getAsTypeRef() {
        return llvm::dyn_cast<TypeRef>(rep);
    }
    DiscreteType *getAsDiscreteType() {
        if (TypeRef *ref = getAsTypeRef()) {
            return llvm::cast<DiscreteType>(ref->getDecl()->getType());
        }
        return 0;
    }
    ComponentDecl *getAsComponent() {
        return llvm::dyn_cast<ComponentDecl>(rep);
    }

    const Expr *getAsExpr() const {
        return llvm::dyn_cast<Expr>(rep);
    }
    const Identifier *getAsIdentifier() const {
        return llvm::dyn_cast<Identifier>(rep);
    }
    const Range *getAsRange() const {
        return llvm::dyn_cast<Range>(rep);
    }
    const TypeRef *getAsTypeRef() const {
        return llvm::dyn_cast<TypeRef>(rep);
    }
    const DiscreteType *getAsDiscreteType() const {
        if (const TypeRef *ref = getAsTypeRef()) {
            return llvm::cast<DiscreteType>(ref->getDecl()->getType());
        }
        return 0;
    }
    const ComponentDecl *getAsComponent() const {
        return llvm::dyn_cast<ComponentDecl>(rep);
    }

    /// Returns the underlying node as a raw Ast.
    Ast *&getRep() { return rep; }
    const Ast *getRep() const { return rep; }
    //@}

    /// \name Representation Setters.
    //@{
    void setKey(Expr          *node) { rep = node; }
    void setKey(Range         *node) { rep = node; }
    void setKey(TypeRef       *node) { rep = node; }
    void setKey(Identifier    *node) { rep = node; }
    void setKey(ComponentDecl *node) { rep = node; }
    //@}

    /// \brief Returns true if this ComponentKey is static.
    ///
    /// A ComponentKey is considered to be static when it is represented by a
    /// static expression, range, or discrete type.  Identifiers are always
    /// considered to be static.
    bool isStatic() const;

    /// \name Bounds Extractors.
    ///
    /// Returns the lower and upper bounds associated with this ComponentKey.
    ///
    /// \note These methods are only valid when this key is not represented by
    /// an identifier and isStatic() is true.
    //@{
    Expr *getLowerExpr();
    Expr *getUpperExpr();

    void getLowerValue(llvm::APInt &value) const;
    void getUpperValue(llvm::APInt &value) const;
    //@}

    /// \name Comparison Predicates.
    //@{

    /// \brief Returns true if this ComponentKey is comparable.
    ///
    /// ComponentKeys are comparable if the underlying representation is a
    /// static expression, range, discrete subtype indication.  ComponentKeys
    /// represented as Identifiers are not considered to be comparable.
    bool isComparable() const { return !denotesIdentifier() && isStatic(); }

    /// \brief Provides an ordering relation between two ComponentKey's for
    /// which isComparable is true.
    ///
    /// Predicate defining a less-than relation between component keys of
    /// unsigned type.  For use with std::stort.
    static bool compareKeysU(const ComponentKey *X, const ComponentKey *Y);

    /// \brief Provides an ordering relation between two ComponentKey's for
    /// which isComparable is true.
    ///
    /// Predicate defining a less-than relation between keys of signed type.
    /// For use with std::stort.
    static bool compareKeysS(const ComponentKey *X, const ComponentKey *Y);
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ComponentKey *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ComponentKey;
    }

private:
    Ast *rep;
    Location loc;
};

//===----------------------------------------------------------------------===//
// ComponentKeyList
//
/// \class
///
/// \brief Represents a list of component keys and an associated expression.
class ComponentKeyList {

public:
    /// Constructs a ComponentKeyList over the given set of ComponentKeys.  At
    /// least one key must be provided.
    static ComponentKeyList *create(ComponentKey **keys, unsigned numKeys,
                                    Expr *expr);

    /// Deallocates a ComponentKeyList.
    static void dispose(ComponentKeyList *CL);

    //@{
    /// Returns the associated expression.
    const Expr *getExpr() const { return expr; }
    Expr *getExpr() { return expr; }
    //@}

    /// Sets the associated expression.
    void setExpr(Expr *expr) { this->expr = expr; }

    /// Returns the number of keys associated with this ComponentKeyList.
    unsigned numKeys() const { return keyCount; }

    /// Returns the location of the first key.
    Location getLocation() const { return keys[0]->getLocation(); }

    /// \name ComponentKey Accessors by Index.
    //@{

    /// Returns the i'th ComponentKey associated with this list.
    const ComponentKey *getKey(unsigned i) const {
        assert(i < keyCount && "Index out of range!");
        return keys[i];
    }
    ComponentKey *&getKey(unsigned i) {
        assert(i < keyCount && "Index out of range!");
        return keys[i];
    }

    /// \brief Returns the actual AST node representing the i'th key cast to the
    /// provided type.
    ///
    /// These methods use llvm::dyn_cast tot resolve the type.  If the key is
    /// not of the supplied type these methods return null.
    template <class T>
    T *resolveKey(unsigned i) {
        assert(i < keyCount && "Index out of range!");
        return llvm::dyn_cast<T>(keys[i]->getRep());
    }

    template <class T>
    const T *resolveKey(unsigned i) const {
        assert(i < keyCount && "Index out of range!");
        return llvm::dyn_cast<T>(keys[i]->getRep());
    }

    /// Returns the i'th key as a raw ast node.
    Ast *&getRawKey(unsigned i) {
        assert(i < keyCount && "Index out of range!");
        return keys[i]->getRep();
    }
    const Ast *getRawKey(unsigned i) const {
        assert(i < keyCount && "Index out of range!");
        return keys[i]->getRep();
    }
    //@}

    /// \name Key Setters.
    //@{
    void setKey(unsigned i, Expr *node) {
        assert(i < keyCount && "Index out of range!");
        keys[i]->setKey(node);
    }
    void setKey(unsigned i, Identifier *node) {
        assert(i < keyCount && "Index out of range!");
        keys[i]->setKey(node);
    }
    void setKey(unsigned i, Range *node) {
        assert(i < keyCount && "Index out of range!");
        keys[i]->setKey(node);
    }
    void setKey(unsigned i, TypeRef *node) {
        assert(i < keyCount && "Index out of range!");
        keys[i]->setKey(node);
    }
    //@}

    //@{
    /// \name Key Iterators.
    ///
    /// \brief Iterators over each key provided by this ComponentKeyList.
    typedef ComponentKey **iterator;
    iterator begin() { return keys; }
    iterator end() { return &keys[keyCount]; }

    typedef ComponentKey *const *const_iterator;
    const_iterator begin() const { return keys; }
    const_iterator end() const { return &keys[keyCount]; }
    //@}

private:
    /// Constructor for use by ComponentKeyList::create.
    ComponentKeyList(ComponentKey **keys, unsigned numKeys, Expr *expr);

    ComponentKeyList(const ComponentKeyList &CL); // do not implement.
    ComponentKeyList &operator =(const ComponentKeyList &CL); // Likewise.

    /// Internally ComponentKeyList's are represented with the actual keys
    /// allocated immediately after the node itself.  Hense \c keys points to
    /// address <tt>this + 1</tt>.
    ComponentKey **keys;
    unsigned keyCount;
    Expr *expr;
};

//===----------------------------------------------------------------------===//
// AggregateExpr
//
/// \class
///
/// \brief Represents general aggregate expressions.
///
/// Comma inherits the aggregate expressions provided by Ada.  These expressions
/// can only be resolved with respect to a type context.  In particular, it is
/// not possible to determine if an aggregate represents an array or record by
/// looking at the syntax alone.  This class is used as a general
/// representation for aggregates of all kinds.
class AggregateExpr : public Expr {

public:
    ~AggregateExpr();

    /// Constructs an empty AggregateExpr.  Components of this aggregate are
    /// introduced via calls to addComponent().
    AggregateExpr(Location loc) : Expr(AST_AggregateExpr, loc), others(0) { }

    /// \brief Returns true if this aggregate is empty.
    ///
    /// An aggregate is empty if there are no components associated with the
    /// expression and there is no others clause.  Aggregate expression nodes
    /// should never be empty in a well-formed Comma program.
    bool empty() const;

    /// \brief Returns true if the indices defined by this aggregate are
    /// statically known.
    ///
    /// In particular, if this aggregate contains any keyed components, the keys
    /// must be static expressions, ranges, or discrete types.  Positional
    /// components are always considered to define static indices.
    ///
    /// \note This predicate is indifferent to whether this aggregate has an
    /// others clause.
    bool hasStaticIndices() const;

    /// \brief If hasStaticIndices is true, returns the total number of
    /// components defined by this aggregate.
    ///
    /// \note For aggregates with a dynamic (non-static) keyed component or an
    /// others clause, this method will return 0.
    ///
    /// \note This method scans the entire aggregate and recomputes the number
    /// of components on each call.  Use sparingly as aggregates can potentially
    /// be quite large.
    unsigned numComponents() const;

    /// \brief Returns true if this aggregate consists of positional components
    /// exclusively.
    ///
    /// \note This predicate is indifferent to whether this aggregate has an
    /// others clause.
    bool isPurelyPositional() const { return !hasKeyedComponents(); }

    /// \brief Returns true if this aggregate consists of keyed components
    /// exclusively.
    ///
    /// \note This predicate is indifferent to whether this aggregate has an
    /// others clause.
    bool isPurelyKeyed() const { return !hasPositionalComponents(); }

    /// \name Positional Components.
    ///
    /// An AggregateExpr node contains a (possibly empty) set of positional
    /// expressions.  The following methods provide access to these components.
    //@{

    /// Returns true if this aggregate contains any positional components.
    bool hasPositionalComponents() const {
        return !positionalComponents.empty();
    }

    /// Returns the number of positional components in this aggregate.
    unsigned numPositionalComponents() const {
        return positionalComponents.size();
    }

    /// \brief Adds a positional component to this aggregate expression.
    ///
    /// The order in which this method is called determines the order of the
    /// components.
    void addComponent(Expr *expr) {
        positionalComponents.push_back(expr);
    }

    /// \name Positional Component Iterators.
    //@{
    typedef std::vector<Expr*>::iterator pos_iterator;
    pos_iterator pos_begin() { return positionalComponents.begin(); }
    pos_iterator pos_end() { return positionalComponents.end(); }

    typedef std::vector<Expr*>::const_iterator const_pos_iterator;
    const_pos_iterator pos_begin() const {
        return positionalComponents.begin();
    }
    const_pos_iterator pos_end() const {
        return positionalComponents.end();
    }
    //@}
    //@}

    /// \name Keyed Components.
    //@{

    /// Adds a ComponentKeyList this KeyedAggExpr.
    void addComponent(ComponentKeyList *keyList) {
        this->keyedComponents.push_back(keyList);
    }

    /// Returns true if this aggregate contains any keyed components.
    bool hasKeyedComponents() const { return !keyedComponents.empty(); }

    /// Returns the number of key lists provided by this aggregate.
    unsigned numKeyLists() const { return keyedComponents.size(); }

    /// Returns the total number of keys provided by this aggregate.
    unsigned numKeys() const;

    /// \name ComponentKeyList Iterators.
    ///
    /// \brief Iterators over the key lists associated with this aggregate.
    //@{
    typedef std::vector<ComponentKeyList*>::iterator kl_iterator;
    kl_iterator kl_begin() { return keyedComponents.begin(); }
    kl_iterator kl_end() { return keyedComponents.end(); }

    typedef std::vector<ComponentKeyList*>::const_iterator const_kl_iterator;
    const_kl_iterator kl_begin() const { return keyedComponents.begin(); }
    const_kl_iterator kl_end() const { return keyedComponents.end(); }
    //@}

private:
    /// \class
    ///
    /// The following class provides a simple forward iterator over each keyed
    /// component provided by an aggregate expression.
    class KeyIterator {
        typedef std::vector<ComponentKeyList*> CKLVector;

        CKLVector &keys;
        unsigned keyIdx;        // Index into the \c keys vector.
        unsigned listIdx;       // Index into the corresponding KeyList.

        /// Internal constructor used for the generation of sentinel iterators.
        KeyIterator(CKLVector &keys,
                    unsigned keyIdx, unsigned listIdx)
            : keys(keys), keyIdx(keyIdx), listIdx(listIdx) { }

        /// Constructor for use by EquivocalAggExpr.
        KeyIterator(CKLVector &keys)
            : keys(keys), keyIdx(0), listIdx(0) { }

        /// Sentinel producing method for use by KeyedAggExpr.
        static KeyIterator getSentinel(CKLVector &keys) {
            return KeyIterator(keys, keys.size(), 0);
        }

        friend class AggregateExpr;

    public:
        typedef ptrdiff_t difference_type;
        typedef ComponentKey *value_type;
        typedef value_type pointer;
        typedef value_type &reference;
        typedef std::forward_iterator_tag iterator_category;

        bool operator ==(const KeyIterator &iter) const {
            return keyIdx == iter.keyIdx && listIdx == iter.listIdx;
        }

        bool operator !=(const KeyIterator &iter) const {
            return !this->operator==(iter);
        }

        KeyIterator &operator++() {
            listIdx = listIdx + 1;
            if (listIdx == keys[keyIdx]->numKeys()) {
                keyIdx = keyIdx + 1;
                listIdx = 0;
            }
            return *this;
        }

        KeyIterator operator++(int) {
            KeyIterator res = *this;
            this->operator++();
            return res;
        }

        reference operator*() {
            return keys[keyIdx]->getKey(listIdx);
        }

        pointer operator->() {
            return keys[keyIdx]->getKey(listIdx);
        }

        /// Returns the expression associated with the current key.
        Expr *getExpr() { return keys[keyIdx]->getExpr(); }
    };

public:
    /// \name Key Iterators.
    ///
    /// \brief Iterators over each key associated with this aggregate.
    ///
    /// These iterators traverse the set of keys in the order they were added to
    /// the aggregate node.
    //@{
    typedef KeyIterator key_iterator;
    key_iterator key_begin() { return KeyIterator(keyedComponents); }
    key_iterator key_end() {
        return KeyIterator::getSentinel(keyedComponents);
    }
    //@}
    //@}

    /// Returns true if this aggregate has a \c others component.
    bool hasOthers() const { return others != 0; }

    //@{
    /// Returns the expression associated with a \c others component or null if
    /// there is no others expression.
    Expr *getOthersExpr() { return others; }
    const Expr *getOthersExpr() const { return others; }
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
        others = component;
    }

    /// Replaces an existing others expression.
    void setOthersExpr(Expr *expr) {
        assert(hasOthers() &&
               "Cannot reset the others expr of this kind of aggregate!");
        others = expr;
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const AggregateExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AggregateExpr;
    }

private:
    // Vector of expressions forming the positional components of this
    // aggregate.  Each positional component is considered to come before any
    // keyed or others components.
    std::vector<Expr*> positionalComponents;

    // Vector of ChoiceList's used to represent the keyed components of this
    // aggregate.  Keyed components come after any positional components.
    std::vector<ComponentKeyList*> keyedComponents;

    // Location of the "others" reserved word.
    Location othersLoc;

    // Expression associated with an "others" component or null if this
    // aggregate does not provide such a component.
    Expr *others;
};

} // end comma namespace.

#endif
