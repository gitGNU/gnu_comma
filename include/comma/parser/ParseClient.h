//===-- parser/ParseClient.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSECLIENT_HDR_GUARD
#define COMMA_PARSECLIENT_HDR_GUARD

#include "comma/basic/ParameterModes.h"
#include "comma/basic/IdentifierInfo.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/PointerIntPair.h"

namespace llvm {

class APInt;

} // end llvm namespace

namespace comma {

class ParseClient;

//===----------------------------------------------------------------------===//
// Node
//
// This class encapsulates (in a non-type-safe manner) the internal data
// structures produced by a ParseClient, and provides automatic memory
// management of that data by way of a reference counting mechanism.
//
// When a ParseClient returns data to the parser, it wraps it up in a Node
// instance.  This provides a uniform, opaque handle on the data which the
// parser can collect and submit back to the ParseClient.
//
// A Node provides automatic memory management using a reference counting
// mechanism.  Nodes can be freely copied and assigned to.  Each Node contains a
// pointer to the ParseClient which produced its associated data.  When the
// reference count drops to zero, ParseClient::deleteNode is called with the
// Node about to be freed, giving the ParseClient the opportunity to manage to
// allocation of its data.
//
// In general, automatic reclamation of nodes occurs when an error is
// encountered during parsing, or during the analysis performed by the
// ParseClient itself.  When the ParseClient accepts a particular construct that
// the parser produces, the associated Node's are typically released, thereby
// inhibiting the automatic reclamation that would otherwise occur.
//
// A Node can be marked as "invalid", meaning that the data which is associated
// with the Node is malformed in some respect.  A ParseClient can return invalid
// nodes to indicate that it could not handle a construct produced by the
// parser.  The parser in turn never submits an invalid Node back to the client.
class Node {

    // This simple structure is used to maintain the ParseClient and reference
    // count associated with each node.
    struct NodeState {

        // Disjoint properties associated with each node, recorded in the low
        // order bits of NodeState::client.
        enum Property {
            None     = 0,      ///< Empty property.
            Invalid  = 1,      ///< Node is invalid.
            Released = 2       ///< Node does not own its payload.
        };

        // The ParseClient associated with this node.  We encode the
        // NodePropertys associated with this node in the low order bits of this
        // field as it is expected to be accessed relatively infrequently.
        llvm::PointerIntPair<ParseClient *, 2> client;

        // The reference count.
        unsigned rc;

        // The payload associated with this node.
        void *payload;

        // NOTE: Constructor initializes the reference count to 1.
        NodeState(ParseClient *client,
                  void *ptr = 0, Property prop = None)
            : client(client, prop),
              rc(1),
              payload(ptr) { }

    private:
        // Do not implement.
        NodeState(const NodeState &state);
        NodeState &operator=(const NodeState &state);
    };

    // Construction of nodes is prohibited except by the ParseClient producing
    // them.  Thus, all direct constructors are private and we define the
    // ParseClient as a friend.
    Node(ParseClient *client, void *ptr, NodeState::Property prop)
        : state(new NodeState(client, ptr, prop)) { }

    Node(ParseClient *client, void *ptr = 0)
        : state(new NodeState(client, ptr)) { }

    static Node getInvalidNode(ParseClient *client) {
        return Node(client, 0, NodeState::Invalid);
    }

    static Node getNullNode(ParseClient *client) {
        return Node(client);
    }

    friend class ParseClient;

public:
    Node(const Node &node)
        : state(node.state) {
        ++state->rc;
    }

    ~Node() { dispose(); }

    Node &operator=(const Node &node);


    // Returns true if this node is invalid.
    bool isInvalid() const {
        return state->client.getInt() & NodeState::Invalid;
    }

    // Returns true if this Node is valid.
    bool isValid() const { return !isInvalid(); }

    // Marks this node as invalid.
    void markInvalid();

    // Returns true if this Node is not associated with any data.
    bool isNull() const {
        return state->payload == 0;
    }

    // Releases ownership of this node (and all copies).
    void release();

    // Returns true if this Node owns the associated pointer.
    bool isOwning();

    // Returns the reference count associated with this node.
    unsigned getRC() { return state->rc; }

    // Returns the pointer associated with this node cast to the supplied type.
    template <class T> static T *lift(Node &node) {
        return reinterpret_cast<T*>(node.state->payload);
    }

private:
    void dispose();

    // Heap-allocated state associated with this node (and all copies).
    NodeState *state;
};

//===----------------------------------------------------------------------===//
// NodeVector
//
// A simple vector type which manages a collection of Node's.  This type
// provides a release method which releases all of the Node's associated with
// the vector.
class NodeVector : public llvm::SmallVector<Node, 16> {
public:
    void release();
};

class ParseClient {

public:
    /// Nodes cannot be constructed from outside a parse client, yet many of the
    /// callbacks take null nodes indicateing a non-existant argument.  This
    /// method is made available to the parser so that it has a means of
    /// producing such null nodes.
    Node getNullNode() { return Node::getNullNode(this); }

    /// Nodes cannot be constructed from outside a parse client.  However, the
    /// parser needs to be able to create invalid nodes to communicate failures
    /// during parsing (just as the ParseClient returns invalid nodes to
    /// indicate a semantic failure).
    Node getInvalidNode() { return Node::getInvalidNode(this); }

    /// Nodes do not know the representation of the data they carry.  This
    /// method is called by Nodes once their reference counts drop to zero.  The
    /// implementation need not delete the node as a result of this call -- it
    /// might choose to cache it, for instance.
    virtual void deleteNode(Node &node) = 0;

    /// \name Initial Callbacks.
    ///
    /// When a top-level capsule is about to be parsed, beginCapsule is invoked
    /// to notify the client of the processing to come.  This is the first
    /// callback the parser ever invokes on its client.  Once the capsule has
    /// been parsed (successfully or not), endCapsule is called.
    ///
    ///@{
    virtual void beginCapsule() = 0;
    virtual void endCapsule() = 0;
    ///@}

    /// \name Generic Formal Callbacks.
    ///
    /// The following callbacks are invoked when processing generic formal
    /// parameters.
    ///
    ///@{
    ///
    /// \name Generic Formal Delimiters.
    ///
    /// Processing of a generic formal part begins with a call to
    /// beginGenericFormals and completes with a call to endGenericFormals.
    /// These calls delimit the scope of a generic formal part to the client.
    ///
    /// @{
    virtual void beginGenericFormals() = 0;
    virtual void endGenericFormals() = 0;
    ///@}

    /// \name Formal Domain Declarations.
    ///
    /// The client is notified of the start of a formal domain declaration with
    /// a call to beginFormalDomainDecl, giving the name and location of the
    /// declaration.  The super signatures and component declarations are then
    /// processed.  The end of a formal domain declaration is announced with a
    /// call to endFormalDomainDecl.
    ///
    ///@{
    virtual void beginFormalDomainDecl(IdentifierInfo *name, Location loc) = 0;
    virtual void endFormalDomainDecl() = 0;
    ///@}
    ///@}

    /// \name Capsule Callbacks.
    ///
    /// Once the generic formal part has been processed (if present at all), one
    /// following callbacks is invoked to inform the client of the type and name
    /// of the upcomming capsule.  The context established by these callbacks is
    /// terminated when endCapsule is called.
    ///
    ///@{
    virtual void beginDomainDecl(IdentifierInfo *name, Location loc) = 0;
    virtual void beginSignatureDecl(IdentifierInfo *name, Location loc) = 0;
    ///@}

    /// \name Signature Profile Callbacks.
    ///
    ///@{
    ///
    /// \name Signature Profile Delimiters
    ///
    /// When the signature profile of a top-level capsule or generic formal
    /// domain is about to be processed, beginSignatureProfile is called.  Once
    /// the processing of the profile is finished (regarless of whether the
    /// parse was successful or not) endSignatureProfile is called.
    ///
    ///@{
    virtual void beginSignatureProfile() = 0;
    virtual void endSignatureProfile() = 0;
    ///@}

    /// Called for each super signature defined in a signature profile.
    virtual void acceptSupersignature(Node typeNode, Location loc) = 0;
    ///@}

    /// Called at the begining of an add expression.  The client accepts
    /// components of an add expression after this call until endAddExpression
    /// is called.
    virtual void beginAddExpression() = 0;

    /// Completes an add expression.
    virtual void endAddExpression() = 0;

    /// Invoked when the parser consumes a carrier declaration.
    virtual void acceptCarrier(IdentifierInfo *name, Node typeNode,
                               Location loc) = 0;


    /// \name Subroutine Declaration Callbacks.
    ///
    /// When a subroutine declaration is about to be parsed, either
    /// beginFunctionDeclaration or beginProcedureDeclaration is invoked to
    /// inform the client of the kind and name of the upcomming subroutine.
    /// Once the declaration has been processed, the context is terminated with
    /// a call to endSubroutineDeclaration.
    ///
    ///@{
    virtual void beginFunctionDeclaration(IdentifierInfo *name,
                                          Location loc) = 0;
    virtual void beginProcedureDeclaration(IdentifierInfo *name,
                                           Location loc) = 0;

    /// When parsing a function declaration, this callback is invoked to notify
    /// the client of the declarations return type.
    ///
    /// If the function declaration was missing a return type, or the node
    /// returned by the client representing the type is invalid, this callback
    /// is passed a null node as argument.  Note that the parser posts a
    /// diagnostic for the case of a missing return.
    virtual void acceptFunctionReturnType(Node typeNode) = 0;

    /// For each subroutine parameter, acceptSubroutineParameter is called
    /// providing:
    ///
    /// \param formal The name of the formal parameter.
    ///
    /// \param loc The location of the formal parameter name.
    ///
    /// \param typeNode A node describing the type of the parameter (the result
    ///  of a call to acceptTypeName or acceptTypeApplication, for example).
    ///
    /// \param mode The parameter mode, wher PM::MODE_DEFAULT is supplied if
    ///  an explicit mode was not parsed.
    ///
    virtual void acceptSubroutineParameter(IdentifierInfo *formal, Location loc,
                                           Node typeNode,
                                           PM::ParameterMode mode) = 0;

    /// Called to notify the client that a subroutine was declared as an
    /// overriding declaration.  The only time this callback is invoked by the
    /// parser is when it is processing a signature profile.
    ///
    /// \param qualNode A node resulting from a call to acceptQualifier or
    /// acceptNestedQualifier, representing the qualification of \p name, or a
    /// null Node if there was no qualification.
    ///
    /// \param name An IdentifierInfo naming the target of the override.
    ///
    /// \param loc The location of \p name.
    virtual void acceptOverrideTarget(Node qualNode,
                                      IdentifierInfo *name, Location loc) = 0;

    /// Called to terminate the context of a subroutine declaration.
    ///
    /// \param definitionFollows Set to true if the parser sees a \c is token
    /// following the declaration and thus expects a definition to follow.
    ///
    /// \return A node associated with the declaration.  Exclusively used by the
    /// parser as an argument to beginSubroutineDefinition.
    virtual Node endSubroutineDeclaration(bool definitionFollows) = 0;
    ///@}

    /// \name Subroutine Definition Callbacks.
    ///
    ///@{
    ///
    /// Once a declaration has been parsed, a context for a definition is
    /// introduced with a call to beginSubroutineDefinition (assuming the
    /// declaration has a definition), passing in the node returned from
    /// endSubroutineDeclaration.
    virtual void beginSubroutineDefinition(Node declarationNode) = 0;

    /// For each statement consituting the body of a definition
    /// acceptSubroutineStmt is invoked with the node provided by any one of the
    /// statement callbacks (acceptIfStmt, acceptReturnStmt, etc) provided that
    /// the Node is valid.  Otherwise, the Node is dropped and this callback is
    /// not invoked.
    virtual void acceptSubroutineStmt(Node stmt) = 0;

    /// Once the body of a subroutine has been parsed, this callback is invoked
    /// to singnal the completion of the definition.
    virtual void endSubroutineDefinition() = 0;
    /// @}

    virtual bool acceptObjectDeclaration(Location loc, IdentifierInfo *name,
                                         Node type, Node initializer) = 0;

    virtual Node acceptPercent(Location loc) = 0;

    virtual Node acceptTypeName(IdentifierInfo *info, Location loc,
                                Node qualNode) = 0;

    virtual Node acceptTypeApplication(IdentifierInfo *connective,
                                       NodeVector &argumentNodes,
                                       Location *argumentLocs,
                                       IdentifierInfo **keys,
                                       Location *keyLocs,
                                       unsigned numKeys,
                                       Location loc) = 0;

    virtual Node acceptKeywordSelector(IdentifierInfo *key, Location loc,
                                       Node exprNode, bool forSubroutine) = 0;

    virtual Node acceptDirectName(IdentifierInfo *name, Location loc,
                                  Node qualNode) = 0;

    virtual Node acceptFunctionName(IdentifierInfo *name, Location loc,
                                    Node qualNode) = 0;

    virtual Node acceptFunctionCall(Node connective, Location loc,
                                    NodeVector &args) = 0;

    virtual Node acceptProcedureName(IdentifierInfo *name, Location loc,
                                     Node qualNode) = 0;

    virtual Node acceptProcedureCall(Node connective, Location loc,
                                     NodeVector &args) = 0;

    /// Called for "inj" expressions.  loc is the location of the inj token and
    /// expr is its argument.
    virtual Node acceptInj(Location loc, Node expr) = 0;

    /// Called for "prj" expressions.  loc is the location of the prj token and
    /// expr is its argument.
    virtual Node acceptPrj(Location loc, Node expr) = 0;

    virtual Node acceptQualifier(Node qualifierType, Location loc) = 0;

    virtual Node acceptNestedQualifier(Node qualifier, Node qualifierType,
                                       Location loc) = 0;

    virtual Node acceptIntegerLiteral(llvm::APInt &value, Location loc) = 0;

    /// Submits an import from the given type node, and the location of the
    /// import reserved word.
    virtual bool acceptImportDeclaration(Node importedType, Location loc) = 0;

    virtual Node acceptIfStmt(Location loc, Node condition,
                              NodeVector &consequents) = 0;

    virtual Node acceptElseStmt(Location loc, Node ifNode,
                                NodeVector &alternates) = 0;

    virtual Node acceptElsifStmt(Location loc, Node ifNode, Node condition,
                                 NodeVector &consequents) = 0;

    virtual Node acceptEmptyReturnStmt(Location loc) = 0;

    virtual Node acceptReturnStmt(Location loc, Node retNode) = 0;

    virtual Node acceptAssignmentStmt(Location loc, IdentifierInfo *target,
                                      Node value) = 0;

    /// Called when a block statement is about to be parsed.
    virtual Node beginBlockStmt(Location loc, IdentifierInfo *label = 0) = 0;

    /// This method is called for each statement associated with the block.
    virtual void acceptBlockStmt(Node block, Node stmt) = 0;

    /// Once the last statement of a block has been parsed, this method is
    /// called to inform the client that we are leaving the block context
    /// established by the last call to beginBlockStmt.
    virtual void endBlockStmt(Node block) = 0;

    /// Called to inform the client of a while statement.
    virtual Node acceptWhileStmt(Location loc, Node condition,
                                 NodeVector &stmtNodes) = 0;

    /// \name Enumeration Callbacks.
    ///
    /// Enumerations are processed by first establishing a context with a call
    /// to beginEnumeration.  For each defining enumeration literal, either
    /// acceptEnumerationIdentifier or acceptEnumerationCharacter is called.
    /// Once all elements of the type have been processed, endEnumeration is
    /// called.
    ///
    ///@{
    ///
    /// Establishes a context begining an enumeration type declaration.
    ///
    /// \param name The name of this enumeration type declaration.
    ///
    /// \param loc The location of the enumerations name.
    ///
    virtual Node beginEnumeration(IdentifierInfo *name, Location loc) = 0;

    /// Called to introduce an enumeration component which was defined using
    /// identifier syntax.
    ///
    /// \param enumeration A Node as returned from beginEnumeration.
    ///
    /// \param name The defining identifier for this component.
    ///
    /// \param loc The location of the defining identifier.
    virtual void acceptEnumerationIdentifier(Node enumeration,
                                             IdentifierInfo *name,
                                             Location loc) = 0;

    /// Called to introduce an enumeration component which was defined using
    /// character syntax.
    ///
    /// \param enumeration A Node as returned from beginEnumeration.
    ///
    /// \param name The name of the character literal defining this component.
    /// This is always the full name of the literal.  For example, the character
    /// literal for \c X is named using the string \c "'X'" (note that the
    /// quotes are included).
    ///
    /// \param loc The location of the defining character literal.
    ///
    virtual void acceptEnumerationCharacter(Node enumeration,
                                            IdentifierInfo *name,
                                            Location loc) = 0;

    /// Called when all of the enumeration literals have been processed, thus
    /// completing the definition of the enumeration.
    ///
    /// \param enumeration A Node as returned from the matching call to
    /// beginEnumeration.
    virtual void endEnumeration(Node enumeration) = 0;
    ///@}

    /// Called to process integer type definitions.
    ///
    /// For example, given a definition of the form <tt>type T is range
    /// X..Y;</tt>, this callback is invoked with \p name set to the identifier
    /// \c T, \p loc set to the location of \p name, \p low set to the
    /// expression \c X, and \p high set to the expression \c Y.
    virtual void acceptIntegerTypedef(IdentifierInfo *name, Location loc,
                                      Node low, Node high) = 0;

protected:
    /// Allow sub-classes to construct arbitrary nodes.
    Node getNode(void *ptr) { return Node(this, ptr); }

    /// Construct a node which has released its ownership to the associated
    /// data.
    Node getReleasedNode(void *ptr) {
        Node node(this, ptr);
        node.release();
        return node;
    }
};

//===----------------------------------------------------------------------===//
// Inline methods.

inline void Node::dispose()
{
    assert(state->rc != 0);
    if (--state->rc == 0) {
        if (isOwning())
            state->client.getPointer()->deleteNode(*this);
        delete state;
    }
}

inline Node &Node::operator=(const Node &node)
{
    if (state != node.state) {
        ++node.state->rc;
        dispose();
        state = node.state;
    }
    return *this;
}

inline void Node::release()
{
    unsigned prop = state->client.getInt();
    state->client.setInt(prop | NodeState::Released);
}

inline bool Node::isOwning()
{
    return !(state->client.getInt() & NodeState::Released);
}

inline void Node::markInvalid()
{
    unsigned prop = state->client.getInt();
    state->client.setInt(prop | NodeState::Invalid);
}

inline void NodeVector::release()
{
    for (iterator iter = begin(); iter != end(); ++iter)
        iter->release();
}

} // End comma namespace.

#endif
