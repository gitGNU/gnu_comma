//===-- parser/ParseStmt.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
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
        else if (blockStmtFollows())
            node = parseBlockStmt();
        else
            node = parseProcedureCallStatement();
        break;

    case Lexer::TKN_IF:
        node = parseIfStmt();
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

    Location loc = ignoreToken();

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
    Node            value(0);

    ignoreToken();              // Ignore the `:='.
    value = parseExpr();
    if (value.isValid())
        return client.acceptAssignmentStmt(location, target, value);
    return Node::getInvalidNode();
}

Node Parser::parseIfStmt()
{
    assert(currentTokenIs(Lexer::TKN_IF));

    Location   loc       = ignoreToken();
    Node       condition = parseExpr();
    NodeVector stmts;
    Node       result(0);

    if (condition.isInvalid() || !requireToken(Lexer::TKN_THEN)) {
        seekEndIf();
        return Node::getInvalidNode();
    }

    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            stmts.push_back(stmt);
    } while (!currentTokenIs(Lexer::TKN_END)   &&
             !currentTokenIs(Lexer::TKN_ELSE)  &&
             !currentTokenIs(Lexer::TKN_ELSIF) &&
             !currentTokenIs(Lexer::TKN_EOT));

    result = client.acceptIfStmt(loc, condition, &stmts[0], stmts.size());
    if (result.isInvalid()) {
        seekEndIf();
        return Node::getInvalidNode();
    }

    while (currentTokenIs(Lexer::TKN_ELSIF)) {
        loc       = ignoreToken();
        condition = parseExpr();
        if (condition.isInvalid() || !requireToken(Lexer::TKN_THEN)) {
            seekEndIf();
            return Node::getInvalidNode();
        }

        stmts.clear();
        do {
            Node stmt = parseStatement();
            if (stmt.isValid())
                stmts.push_back(stmt);
        } while (!currentTokenIs(Lexer::TKN_END) &&
                 !currentTokenIs(Lexer::TKN_ELSE) &&
                 !currentTokenIs(Lexer::TKN_ELSIF) &&
                 !currentTokenIs(Lexer::TKN_EOT));

        result = client.acceptElsifStmt(loc, result, condition,
                                        &stmts[0], stmts.size());

        if (result.isInvalid()) {
            seekEndIf();
            return Node::getInvalidNode();
        }
    }

    if (currentTokenIs(Lexer::TKN_ELSE)) {
        loc = ignoreToken();
        stmts.clear();
        do {
            Node stmt = parseStatement();
            if (stmt.isValid())
                stmts.push_back(stmt);
        } while (!currentTokenIs(Lexer::TKN_END)  &&
                 !currentTokenIs(Lexer::TKN_EOT));

        result = client.acceptElseStmt(loc, result, &stmts[0], stmts.size());
    }

    if (!requireToken(Lexer::TKN_END) || !requireToken(Lexer::TKN_IF))
        return Node::getInvalidNode();

    return Node(result);
}

Node Parser::parseBlockStmt()
{
    Node            block(0);
    Location        loc   = currentLocation();
    IdentifierInfo *label = 0;

    assert(blockStmtFollows());

    if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        // Parse this blocks label.
        label = parseIdentifierInfo();
        ignoreToken();
        block = client.beginBlockStmt(loc, label);
    }
    else
        block = client.beginBlockStmt(loc);

    if (reduceToken(Lexer::TKN_DECLARE)) {
        while (!currentTokenIs(Lexer::TKN_BEGIN) &&
               !currentTokenIs(Lexer::TKN_EOT)) {
            parseDeclaration();
            requireToken(Lexer::TKN_SEMI);
        }
    }

    if (requireToken(Lexer::TKN_BEGIN)) {
        while (!currentTokenIs(Lexer::TKN_END) &&
               !currentTokenIs(Lexer::TKN_EOT)) {
            Node stmt = parseStatement();
            if (stmt.isValid())
                client.acceptBlockStmt(block, stmt);
        }
        if (parseEndTag(label)) {
            client.endBlockStmt(block);
            return block;
        }
    }

    seekAndConsumeEndTag(label);
    return Node::getInvalidNode();
}
