//===-- ast/Descriptors.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
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

void Node::markInvalid()
{
    unsigned prop = state->client.getInt();
    state->client.setInt(prop | NodeState::Invalid);
}

void NodeVector::release()
{
    for (iterator iter = begin(); iter != end(); ++iter)
        iter->release();
}
