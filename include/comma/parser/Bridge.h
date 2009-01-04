//===-- parser/Bridge.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_BRIDGE_HDR_GUARD
#define COMMA_PARSER_BRIDGE_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "llvm/Support/DataTypes.h"

namespace comma {

class Node {

public:
    // Constructs an invalid node.
    Node() : payload(1) { }
    Node(void *ptr) : payload(reinterpret_cast<uintptr_t>(ptr)) { }

    // Returns true if this node is invalid.
    bool isInvalid() const { return payload & 1u; }

    // Returns true if this Node is valid.
    bool isValid() const { return !isInvalid(); }

    // Returns the pointer associated with this node cast to the supplied type.
    template <class T> static T *lift(Node &node) {
        return reinterpret_cast<T*>(node.payload & ~((uintptr_t)1));
    }

private:
    uintptr_t payload;
};


class Bridge {

public:
    // The parser does not know about the underlying node representation.  It
    // calls this method when cleaning up after a bad parse.  The implementation
    // need not delete the node as a result of this call -- it might choose to
    // cache it, for instance.
    virtual void deleteNode(Node node) = 0;

    // Enumerations which indicate what kind of model this bridge is operating
    // over.
    enum DefinitionKind {
        Signature,
        Variety,
        Domain,
        Functor
    };

    // Processing of models begin with a call to beginModelDefinition, and
    // are completed with a call to endModelDefinition.
    virtual void
    beginModelDefinition(DefinitionKind kind,
                         IdentifierInfo *id, Location location) = 0;

    virtual void endModelDefinition() = 0;

    // Called immediately after a model definition has been registered.  This
    // call defines a formal parameter of the model (which must be either a
    // functor or variety). The order of the calls determines the shape of the
    // models formal parameter list.
    virtual void acceptModelParameter(IdentifierInfo *formal,
                                      Node typeNode, Location loc) = 0;

    // Once the formal parameters have been accepted, this function is called to
    // begin processing of a models supersignature list.
    virtual void beginModelSupersignatures() = 0;

    // For each supersignature, this function is called to notify the type
    // checker of the existance of a supersignature.
    virtual void acceptModelSupersignature(Node typeNode,
                                           const Location loc) = 0;

    // Called immediately after all supersignatures have been processed.
    virtual void endModelSupersignatures() = 0;

    virtual Node acceptPercent(Location loc) = 0;

    virtual Node acceptTypeIdentifier(IdentifierInfo *info, Location loc) = 0;

    virtual Node acceptTypeApplication(IdentifierInfo *connective,
                                       Node *argumentNodes, unsigned numArgs,
                                       Location loc) = 0;
};

};

#endif
