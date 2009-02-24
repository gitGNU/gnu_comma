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
    Node node = parseProcedureCallStatement();
    return node;
}

Node Parser::parseProcedureCallStatement()
{
    Location        loc  = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name) return Node::getInvalidNode();

    NodeVector arguments;
    bool       seenSelector = false;

    // If we have an empty set paramters, treat as a nullary name.
    if (unitExprFollows()) {
        report(diag::ILLEGAL_EMPTY_PARAMS);
        ignoreToken();
        ignoreToken();
        return client.acceptProcedureCall(name, loc, 0, 0);
    }

    if (reduceToken(Lexer::TKN_LPAREN)) {
        do {
            Node arg;
            if (keywordSelectionFollows()) {
                arg = parseSubroutineKeywordSelection();
                seenSelector = true;
            }
            else if (seenSelector) {
                report(diag::POSITIONAL_FOLLOWING_SELECTED_PARAMETER);
                seekCloseParen();
                return Node::getInvalidNode();
            }
            else
                arg = parseExpr();

            if (arg.isInvalid()) {
                seekCloseParen();
                return Node::getInvalidNode();
            }
            arguments.push_back(arg);
        } while (reduceToken(Lexer::TKN_COMMA));

        if (!requireToken(Lexer::TKN_RPAREN)) {
            seekCloseParen();
            return Node::getInvalidNode();
        }
    }

    return client.acceptProcedureCall(name,
                                      loc,
                                      &arguments[0],
                                      arguments.size());
}
