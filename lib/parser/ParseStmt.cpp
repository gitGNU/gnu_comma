//===-- parser/ParseStmt.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/Pragmas.h"
#include "comma/parser/Parser.h"

#include <cassert>
#include <cstring>

using namespace comma;

Node Parser::parseStatement()
{
    Node node = getInvalidNode();

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

    case Lexer::TKN_WHILE:
        node = parseWhileStmt();
        break;

    case Lexer::TKN_FOR:
        node = parseForStmt();
        break;

    case Lexer::TKN_RETURN:
        node = parseReturnStmt();
        break;

    case Lexer::TKN_PRAGMA:
        node = parsePragmaStmt();
        break;
    }

    if (node.isInvalid() || !requireToken(Lexer::TKN_SEMI))
        seekAndConsumeToken(Lexer::TKN_SEMI);

    return node;
}

Node Parser::parseProcedureCallStatement()
{
    Node name = parseName(Statement_Name);
    if (name.isValid())
        return client.acceptProcedureCall(name);
    return getInvalidNode();
}

Node Parser::parseReturnStmt()
{
    assert(currentTokenIs(Lexer::TKN_RETURN));

    Location loc = ignoreToken();

    if (currentTokenIs(Lexer::TKN_SEMI))
        return client.acceptEmptyReturnStmt(loc);

    Node expr = parseExpr();

    if (expr.isValid())
        return client.acceptReturnStmt(loc, expr);
    return expr;
}

Node Parser::parseAssignmentStmt()
{
    assert(assignmentFollows());

    Node target = parseName();

    if (target.isInvalid())
        return getInvalidNode();

    ignoreToken();              // Ignore the `:='.

    Node value = parseExpr();

    if (value.isValid())
        return client.acceptAssignmentStmt(target, value);

    return getInvalidNode();
}

Node Parser::parseIfStmt()
{
    assert(currentTokenIs(Lexer::TKN_IF));

    Location   loc       = ignoreToken();
    Node       condition = parseExpr();
    NodeVector stmts;

    if (condition.isInvalid() || !requireToken(Lexer::TKN_THEN)) {
        seekEndIf();
        return getInvalidNode();
    }

    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            stmts.push_back(stmt);
    } while (!currentTokenIs(Lexer::TKN_END)   &&
             !currentTokenIs(Lexer::TKN_ELSE)  &&
             !currentTokenIs(Lexer::TKN_ELSIF) &&
             !currentTokenIs(Lexer::TKN_EOT));

    Node result = client.acceptIfStmt(loc, condition, stmts);
    if (result.isInvalid()) {
        seekEndIf();
        return getInvalidNode();
    }

    while (currentTokenIs(Lexer::TKN_ELSIF)) {
        loc       = ignoreToken();
        condition = parseExpr();
        if (condition.isInvalid() || !requireToken(Lexer::TKN_THEN)) {
            seekEndIf();
            return getInvalidNode();
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

        result = client.acceptElsifStmt(loc, result, condition, stmts);

        if (result.isInvalid()) {
            seekEndIf();
            return getInvalidNode();
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

        result = client.acceptElseStmt(loc, result, stmts);
    }

    if (!requireToken(Lexer::TKN_END) || !requireToken(Lexer::TKN_IF))
        return getInvalidNode();

    return Node(result);
}

Node Parser::parseBlockStmt()
{
    Location        loc   = currentLocation();
    IdentifierInfo *label = 0;

    assert(blockStmtFollows());

    // Parse this blocks label, if available.
    if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        label = parseIdentifierInfo();
        ignoreToken();          // Ignore the ":".
    }

    Node block = client.beginBlockStmt(loc, label);

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
    return getInvalidNode();
}

Node Parser::parseWhileStmt()
{
    assert(currentTokenIs(Lexer::TKN_WHILE));

    Location loc = ignoreToken();
    Node condition = parseExpr();
    NodeVector stmts;

    if (condition.isInvalid() || !requireToken(Lexer::TKN_LOOP)) {
        seekEndLoop();
        return getInvalidNode();
    }

    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            stmts.push_back(stmt);
    } while (!currentTokenIs(Lexer::TKN_END) &&
             !currentTokenIs(Lexer::TKN_EOT));

    if (!requireToken(Lexer::TKN_END) || !requireToken(Lexer::TKN_LOOP))
        return getInvalidNode();

    return client.acceptWhileStmt(loc, condition, stmts);
}

Node Parser::parseForStmt()
{
    assert(currentTokenIs(Lexer::TKN_FOR));
    Location forLoc = ignoreToken();

    Location iterLoc = currentLocation();
    IdentifierInfo *iterName = parseIdentifierInfo();

    if (!iterName || !requireToken(Lexer::TKN_IN)) {
        seekEndLoop();
        return getInvalidNode();
    }

    bool isReversed = reduceToken(Lexer::TKN_REVERSE);

    Lexer::Token savedToken = currentToken();
    lexer.beginExcursion();

    // Attempt to consume a name from the token stream.  If the parse is
    // successful, store the code of the token immediately following the name in
    // ctxCode.
    Lexer::Code ctxCode = Lexer::UNUSED_ID;

    if (consumeName())
        ctxCode = currentTokenCode();

    lexer.endExcursion();
    setCurrentToken(savedToken);

    // Inspect the context code to determine the next action.  We have the
    // following cases:
    //
    //    - TKN_LOOP : The name constitues the entire control expression,
    //      which means it is either a simple subtype mark or a range
    //      attribute.
    //
    //    - TKN_DDOT : The name consitutes the lower bound of a range.
    //
    //    - TKN_RANGE, TKN_DIGITS, TKN_DELTA : The name denotes a subtype
    //      mark in a discrete subtype indication.
    //
    // FIXME: Note that we do not support the last case yet.  As a consequence,
    // the following code does not use the context information fully.  We simply
    // check for the first case and assume the second, for now.
    Node forNode = getNullNode();
    if (ctxCode == Lexer::TKN_LOOP) {
        Node control = parseName(Accept_Range_Attribute);

        if (control.isInvalid() || !requireToken(Lexer::TKN_LOOP)) {
            seekEndLoop();
            return getInvalidNode();
        }

        forNode = client.beginForStmt(forLoc, iterName, iterLoc,
                                      control, isReversed);
    }
    else {
        // FIXME:  We should be parsing simple expressions here.
        Node lower = parseExpr();
        if (lower.isInvalid() || !requireToken(Lexer::TKN_DDOT)) {
            seekEndLoop();
            return getInvalidNode();
        }

        Node upper = parseExpr();
        if (upper.isInvalid() || !requireToken(Lexer::TKN_LOOP)) {
            seekEndLoop();
            return getInvalidNode();
        }

        forNode = client.beginForStmt(forLoc, iterName, iterLoc,
                                      lower, upper, isReversed);
    }

    NodeVector stmts;
    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            stmts.push_back(stmt);
    } while (!currentTokenIs(Lexer::TKN_END) &&
             !currentTokenIs(Lexer::TKN_EOT));

    // Provide the client with the complete set of statements immediately since
    // we must match the call to beginForStmt with the following call to
    // endForStmt;
    forNode = client.endForStmt(forNode, stmts);

    if (!requireToken(Lexer::TKN_END) || !requireToken(Lexer::TKN_LOOP))
        return getInvalidNode();

    return forNode;
}

Node Parser::parsePragmaStmt()
{
    assert(currentTokenIs(Lexer::TKN_PRAGMA));
    ignoreToken();

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name)
        return getInvalidNode();

    llvm::StringRef ref(name->getString());
    pragma::PragmaID ID = pragma::getPragmaID(ref);

    if (ID == pragma::UNKNOWN_PRAGMA) {
        report(loc, diag::UNKNOWN_PRAGMA) << name;
        return getInvalidNode();
    }

    // Currently, the only pragma accepted in a statement context is Assert.
    // When the set of valid pragmas expands, special parsers will be written to
    // parse the arguments.
    switch (ID) {
    default:
        report(loc, diag::INVALID_PRAGMA_CONTEXT) << name;
        return getInvalidNode();

    case pragma::Assert:
        return parsePragmaAssert(name, loc);
    }
}

Node Parser::parsePragmaAssert(IdentifierInfo *name, Location loc)
{
    if (requireToken(Lexer::TKN_LPAREN)) {
        NodeVector args;
        Node condition = parseExpr();

        if (condition.isInvalid() || !requireToken(Lexer::TKN_RPAREN)) {
            seekTokens(Lexer::TKN_RPAREN);
            return getInvalidNode();
        }

        args.push_back(condition);
        return client.acceptPragmaStmt(name, loc, args);
    }
   return getInvalidNode();
}
