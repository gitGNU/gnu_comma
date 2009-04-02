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
    Node node = Node::getInvalidNode();

    switch (currentTokenCode()) {

    default:
        if (assignmentFollows())
            node = parseAssignmentStmt();
        else
            node = parseProcedureCallStatement();
        break;

    case Lexer::TKN_RETURN:
        node = parseReturnStmt();
        break;
    }

    if (node.isInvalid() || !requireToken(Lexer::TKN_SEMI))
        seekAndConsumeToken(Lexer::TKN_SEMI);

    return node;
}

Node Parser::parseProcedureCallStatement()
{
    Location        loc;
    IdentifierInfo *name;

    // FIXME:  Use result of qualification parsing.
    if (qualificationFollows())
        parseQualificationExpr();

    loc  = currentLocation();
    name = parseIdentifierInfo();

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
            Node arg = Node::getInvalidNode();
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

Node Parser::parseReturnStmt()
{
    assert(currentTokenIs(Lexer::TKN_RETURN));

    Location loc = currentLocation();
    ignoreToken();

    if (currentTokenIs(Lexer::TKN_SEMI))
        return client.acceptReturnStmt(loc);

    Node expr = parseExpr();

    if (expr.isValid())
        return client.acceptReturnStmt(loc, expr);
    return expr;
}

Node Parser::parseAssignmentStmt()
{
    assert(assignmentFollows());

    Location        location = currentLocation();
    IdentifierInfo *target   = parseIdentifierInfo();
    ignoreToken();              // Ignore the `:='.
    Node value = parseExpr();

    if (value.isValid())
        return client.acceptAssignmentStmt(location, target, value);
    return Node::getInvalidNode();
}


