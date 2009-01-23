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

    static Node getInvalidNode() { return Node(); }

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

    // Processing of models begin with a call to beginSignatureDefinition or
    // beginDomainDefinition.  Precessing is completed with a call to
    // endModelDefinition().
    virtual void beginSignatureDefinition(IdentifierInfo *name,
                                          Location loc) = 0;

    virtual void beginDomainDefinition(IdentifierInfo *name, Location loc) = 0;

    virtual void endModelDefinition() = 0;

    // Called immediately after a model definition has been registered.  This
    // call defines a formal parameter of the model (which must be either a
    // functor or variety). The parser collects the results of this call and
    // then supplies them back to the bridge in a call to
    // acceptFormalParameterList.
    virtual Node acceptModelParameter(IdentifierInfo *formal,
                                      Node typeNode, Location loc) = 0;

    // This call completes the definition of a models formal parameters.  This
    // function is called for every definition.  If the definition was without
    // formal parameters, then the supplied lists in this call are empty (arity
    // is zero).
    virtual void acceptModelParameterList(Node *params,
                                          Location *locs, unsigned arity) = 0;

    // Once the formal parameters have been accepted, this function is called
    // for each direct super signature parsed.  The result node of this function
    // are collected by the parser and supplied to the bridge in a final call to
    // acceptModelSupersignatureList once all direct supersignatures have been
    // processed.
    virtual Node acceptModelSupersignature(Node typeNode, Location loc) = 0;

    // Provides the final list of direct supersignatures as returned by calss to
    // acceptModelSupersignature.  If the model does not contain any
    // supersignatures, this function is still invoked by the parser with
    // numSigs set to 0.
    virtual void acceptModelSupersignatureList(Node *sigs,
                                               unsigned numSigs) = 0;

    virtual Node acceptSignatureComponent(IdentifierInfo *name,
                                          Node typeNode, Location loc) = 0;

    virtual void acceptSignatureComponentList(Node *components,
                                              unsigned numComponents) = 0;


    // Called at the begining of an add expression.  The bridge accepts
    // components of an add expression after this call until endAddExpression is
    // called.
    virtual void beginAddExpression() { }

    // Completes an add expression.
    virtual void endAddExpression() { }

    virtual Node beginFunctionDefinition(IdentifierInfo *name,
                                         Node            type,
                                         Location        loc)
        { return Node(0); }

    virtual void acceptFunctionDefinition(Node function, Node body) { }

    virtual void acceptDeclaration(IdentifierInfo *name,
                                   Node            type,
                                   Location        loc) { }

    virtual Node acceptPercent(Location loc) = 0;

    virtual Node acceptTypeIdentifier(IdentifierInfo *info, Location loc) = 0;

    virtual Node acceptTypeApplication(IdentifierInfo  *connective,
                                       Node            *argumentNodes,
                                       Location        *argumentLocs,
                                       unsigned         numArgs,
                                       IdentifierInfo **selectors,
                                       Location        *selectorLocs,
                                       unsigned         numSelectors,
                                       Location         loc) = 0;

    // Functions are built up thru a sequence of calls.  First is to establish
    // the type of the function via a call to acceptFunctionType.  This function
    // takes a set of Nodes representing the argument types which the function
    // accepts, and a Node representing the return type.
    //
    // The raw function type node is then elaborated with a set of formal parameters


    virtual Node acceptFunctionType(IdentifierInfo **formals,
                                    Location        *formalLocations,
                                    Node            *types,
                                    Location        *typeLocations,
                                    unsigned         arity,
                                    Node             returnType,
                                    Location         returnLocation) = 0;
};

} // End comma namespace.

#endif
