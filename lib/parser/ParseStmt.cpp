//===-- parser/ParseStmt.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"
#include <cassert>

using namespace comma;

Node Parser::parseStatement()
{
    assert(false && "Statement parsing not yet implemented!");
    return Node::getInvalidNode();
}


void Parser::parseImportStatement()
{
    Location location = currentLocation();
    Node importedType;

    assert(currentTokenIs(Lexer::TKN_IMPORT));
    ignoreToken();

    importedType = parseModelInstantiation();

    if (importedType.isValid())
        client.acceptImportStatement(importedType, location);
}
