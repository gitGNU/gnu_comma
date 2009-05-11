//===-- ast/Descriptors.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Descriptors.h"
#include "comma/parser/ParseClient.h"

using namespace comma;


void Node::dispose()
{
    assert(state->rc != 0);
    if (--state->rc == 0) {
        if (isOwning())
            state->client.getPointer()->deleteNode(*this);
        delete state;
    }
}

Node &Node::operator=(const Node &node)
{
    if (state != node.state) {
        ++node.state->rc;
        dispose();
        state = node.state;
    }
    return *this;
}

void Node::release()
{
    unsigned prop = state->client.getInt();
    state->client.setInt(prop | NodeState::Released);
}

bool Node::isOwning()
{
    return !(state->client.getInt() & NodeState::Released);
}


void NodeVector::release()
{
    for (iterator iter = begin(); iter != end(); ++iter)
        iter->release();
}


Descriptor::Descriptor(ParseClient *client, DescriptorKind kind)
    : kind (kind),
      invalidFlag(false),
      hasReturn(false),
      client(client),
      idInfo(0),
      location(0),
      returnType(client, 0) { }

void Descriptor::setReturnType(Node node)
{
    assert(kind == DESC_Function &&
           "Only function descriptors have return types!");
    returnType = node;
    hasReturn  = true;
}

Node Descriptor::getReturnType()
{
    assert(kind == DESC_Function &&
           "Only function descriptors have return types!");
    assert(hasReturn &&
           "Return type not set for this function descriptor!");
    return returnType;
}

void Descriptor::clear()
{
    kind        = DESC_Empty;
    invalidFlag = false;
    hasReturn   = false;
    idInfo      = 0;
    location    = 0;
    returnType  = Node(client, 0);
    params.clear();
}

