//===-- parser/parser.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"
#include <cassert>

using namespace comma;

Node Parser::parseBeginExpr(IdentifierInfo *endTag)
{
    seekAndConsumeEndTag(endTag);
    return 0;
}

Node Parser::parseImplicitDeclareExpr(IdentifierInfo *endTag)
{
    seekAndConsumeEndTag(endTag);
    return 0;
}
