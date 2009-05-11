//===-- parser/Descriptors.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_DESCRIPTORS_HDR_GUARD
#define COMMA_PARSER_DESCRIPTORS_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "comma/basic/Location.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/PointerIntPair.h"

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
        NodeState(NodeState &state);
        NodeState &operator=(const NodeState &state);
    };

    Node(ParseClient *client, void *ptr, NodeState::Property prop)
        : state(new NodeState(client, ptr, prop)) { }

public:
    Node(ParseClient *client, void *ptr = 0)
        : state(new NodeState(client, ptr)) { }

    Node(const Node &node)
        : state(node.state) {
        ++state->rc;
    }

    static Node getInvalidNode(ParseClient *client) {
        return Node(client, 0, NodeState::Invalid);
    }

    static Node getNullNode(ParseClient *client) {
        return Node(client);
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


class Descriptor {

    /// The type used to hold nodes representing the formal parameters of this
    /// descriptor.
    typedef NodeVector paramVector;

public:

    enum DescriptorKind {
        DESC_Empty,             ///< Kind of uninitialized descriptors.
        DESC_Signature,         ///< Signature descriptors.
        DESC_Domain,            ///< Domain descriptors.
        DESC_Function,          ///< Function descriptors.
        DESC_Procedure          ///< Procedure descriptors.
    };

    /// Creates a descriptor object of the given kind.  The DescriptorKind
    /// argument defaults to DESC_Empty, meaning the descriptor type must be set
    /// (via a call to initialize) before it can be used.
    Descriptor(ParseClient *client, DescriptorKind kind = DESC_Empty);

    /// Sets this Descriptor to an uninitialized state allowing it to be reused
    /// to represent a new element.  On return, the kind of this descriptor is
    /// set to DESC_Empty.
    void clear();

    /// Clears the current state of this Descriptor and sets is type to the
    /// given kind.
    void initialize(DescriptorKind descKind) {
        clear();
        kind = descKind;
    }

    /// Releases all Node's associated with this descriptor.
    void release();

    /// Associates the given identifier and location with this descriptor.
    void setIdentifier(IdentifierInfo *id, Location loc) {
        idInfo   = id;
        location = loc;
    }

    /// Returns the kind of this descriptor.
    DescriptorKind getKind() const { return kind; }

    /// Predicate functions for enquiering about this descriptors kind.
    bool isFunctionDescriptor()  const { return kind == DESC_Function; }
    bool isProcedureDescriptor() const { return kind == DESC_Procedure; }
    bool isSignatureDescriptor() const { return kind == DESC_Signature; }
    bool isDomainDescriptor()    const { return kind == DESC_Domain; }

    /// Retrives the identifier associated with this descriptor.
    IdentifierInfo *getIdInfo() const { return idInfo; }

    /// Retrives the location associated with this descriptor.
    Location getLocation() const { return location; }

    bool isValid() const { return !invalidFlag; }
    bool isInvalid() const { return invalidFlag; }
    void setInvalid() { invalidFlag = true; }

    /// Adds a Node to this descriptor representing the formal parameter of a
    /// model or subroutine.
    void addParam(Node param) { params.push_back(param); }

    /// Returns the number of parameters currently associated with this
    /// descriptor.
    unsigned numParams() const { return params.size(); }

    /// Returns true if parameters have been associated with this descriptor.
    bool hasParams() const { return numParams() > 0; }

    typedef paramVector::iterator paramIterator;
    paramIterator beginParams() { return params.begin(); }
    paramIterator endParams()   { return params.end(); }

    /// Sets the return type of this descriptor.  Return types can only be
    /// associated with function descriptors.  This method will assert if this
    /// descriptor is of any other kind.
    void setReturnType(Node node);

    /// Accesses the return type of this descriptor.  A call to this function is
    /// valid iff the descriptor denotes a function and setReturnType has been
    /// called.  If these conditions do not hold then this method will assert.
    Node getReturnType();

private:
    DescriptorKind  kind        : 8;
    bool            invalidFlag : 1;
    bool            hasReturn   : 1;
    ParseClient    *client;
    IdentifierInfo *idInfo;
    Location        location;
    paramVector     params;
    Node            returnType;
};

} // End comma namespace.

#endif
