//===-- parser/Descriptors.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_DESCRIPTORS_HDR_GUARD
#define COMMA_PARSER_DESCRIPTORS_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "comma/basic/Location.h"
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

} // End comma namespace.

#endif
