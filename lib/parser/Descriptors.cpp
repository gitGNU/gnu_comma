//===-- ast/Descriptors.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Descriptors.h"

using namespace comma;

Descriptor::Descriptor()
    : kind(DESC_Empty),
      invalidFlag(false),
      hasReturn(false),
      idInfo(0),
      location(0),
      returnType(0) { }

Descriptor::Descriptor(DescriptorKind kind)
    : kind (kind),
      invalidFlag(false),
      hasReturn(false),
      idInfo(0),
      location(0),
      returnType(0) { }

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
    returnType  = Node(0);
    params.clear();
}

