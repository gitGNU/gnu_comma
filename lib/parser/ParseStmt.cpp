//===-- parser/ParseStmt.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
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
        else if (taggedStmtFollows())
            node = parseTaggedStmt();
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

    case Lexer::TKN_LOOP:
        node = parseLoopStmt();
        break;

    case Lexer::TKN_RETURN:
        node = parseReturnStmt();
        break;

    case Lexer::TKN_DECLARE:
    case Lexer::TKN_BEGIN:
        node = parseBlockStmt();
        break;

    case Lexer::TKN_EXIT:
        node = parseExitStmt();
        break;

    case Lexer::TKN_RAISE:
        node = parseRaiseStmt();
        break;

    case Lexer::TKN_PRAGMA:
        node = parsePragmaStmt();
        break;

    case Lexer::TKN_NULL:
        node = client.acceptNullStmt(ignoreToken());
        break;
    }

    if (!requireToken(Lexer::TKN_SEMI))
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

    seekSemi();
    return getInvalidNode();
}

Node Parser::parseAssignmentStmt()
{
    assert(assignmentFollows());

    Node target = parseName();

    if (target.isInvalid()) {
        seekSemi();
        return getInvalidNode();
    }

    ignoreToken();              // Ignore the `:='.

    Node value = parseExpr();

    if (value.isValid())
        return client.acceptAssignmentStmt(target, value);
    else {
        seekSemi();
        return getInvalidNode();
    }
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

Node Parser::parseTaggedStmt()
{
    assert(currentTokenIs(Lexer::TKN_IDENTIFIER) &&
           nextTokenIs(Lexer::TKN_COLON));

    Location tagLoc = currentLocation();
    IdentifierInfo *tag = parseIdentifier();

    ignoreToken();              // Ignore the :
    Node result = getInvalidNode();

    switch (currentTokenCode()) {

    default:
        report(diag::UNEXPECTED_TOKEN) << currentTokenString();
        seekSemi();
        break;
        return getInvalidNode();

    case Lexer::TKN_DECLARE:
    case Lexer::TKN_BEGIN:
        result = parseBlockStmt(tag, tagLoc);
        break;

    case Lexer::TKN_FOR:
        result = parseForStmt(tag, tagLoc);
        break;

    case Lexer::TKN_WHILE:
        result = parseWhileStmt(tag, tagLoc);
        break;

    case Lexer::TKN_LOOP:
        result = parseLoopStmt(tag, tagLoc);
        break;
    }
    return result;
}

Node Parser::parseBlockStmt(IdentifierInfo *tag, Location loc)
{
    if (!tag)
        loc = currentLocation();

    Node block = client.beginBlockStmt(loc, tag);

    if (reduceToken(Lexer::TKN_DECLARE)) {
        while (!currentTokenIs(Lexer::TKN_BEGIN) &&
               !currentTokenIs(Lexer::TKN_EOT)) {
            parseDeclaration();
            requireToken(Lexer::TKN_SEMI);
        }
    }

    if (!requireToken(Lexer::TKN_BEGIN)) {
        client.endBlockStmt(block);
        seekAndConsumeEndTag(tag);
        return getInvalidNode();
    }

    // Consume each statement in the block.
    while (!currentTokenIs(Lexer::TKN_END) &&
           !currentTokenIs(Lexer::TKN_EXCEPTION) &&
           !currentTokenIs(Lexer::TKN_EOT)) {
        Node stmt = parseStatement();
        if (stmt.isValid())
            client.acceptStmt(block, stmt);
    }

    // Inform the client all statements have been consumed.
    client.endBlockStmt(block);

    // Process any exception handlers.
    if (currentTokenIs(Lexer::TKN_EXCEPTION))
        parseExceptionStmt(block);

    // Finish up.
    if (!parseEndTag(tag))
        seekAndConsumeEndTag(tag);
    return block;
}

Node Parser::parseWhileStmt(IdentifierInfo *tag, Location tagLoc)
{
    assert(currentTokenIs(Lexer::TKN_WHILE));

    Location loc = ignoreToken();
    Node condition = parseExpr();

    if (condition.isInvalid() || !requireToken(Lexer::TKN_LOOP)) {
        seekEndLoop();
        return getInvalidNode();
    }

    Node whileNode = client.beginWhileStmt(loc, condition, tag, loc);

    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            client.acceptStmt(whileNode, stmt);
    } while (!currentTokenIs(Lexer::TKN_END) &&
             !currentTokenIs(Lexer::TKN_EOT));

    if (!parseLoopEndTag(tag))
        seekEndLoop(tag);

    return client.endWhileStmt(whileNode);
}

Node Parser::parseLoopStmt(IdentifierInfo *tag, Location tagLoc)
{
    assert(currentTokenIs(Lexer::TKN_LOOP));

    Location loc = ignoreToken();
    Node loopNode = client.beginLoopStmt(loc, tag, tagLoc);

    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            client.acceptStmt(loopNode, stmt);
    } while (!currentTokenIs(Lexer::TKN_END) &&
             !currentTokenIs(Lexer::TKN_EOT));

    if (!parseLoopEndTag(tag))
        seekEndLoop(tag);

    return client.endLoopStmt(loopNode);
}

Node Parser::parseForStmt(IdentifierInfo *tag, Location tagLoc)
{
    assert(currentTokenIs(Lexer::TKN_FOR));
    Location forLoc = ignoreToken();

    Location iterLoc = currentLocation();
    IdentifierInfo *iterName = parseIdentifier();

    if (!iterName || !requireToken(Lexer::TKN_IN)) {
        seekEndLoop();
        return getInvalidNode();
    }

    bool isReversed = reduceToken(Lexer::TKN_REVERSE);
    Node DST = parseDSTDefinition(false);
    if (DST.isInvalid() || !requireToken(Lexer::TKN_LOOP)) {
        seekEndLoop();
        return getInvalidNode();
    }

    Node forNode = client.beginForStmt
        (forLoc, iterName, iterLoc, DST, isReversed, tag, tagLoc);

    do {
        Node stmt = parseStatement();
        if (stmt.isValid())
            client.acceptStmt(forNode, stmt);
    } while (!currentTokenIs(Lexer::TKN_END) &&
             !currentTokenIs(Lexer::TKN_EOT));

    if (!parseLoopEndTag(tag))
        seekEndLoop(tag);

    return client.endForStmt(forNode);
}

Node Parser::parseExitStmt()
{
    assert(currentTokenIs(Lexer::TKN_EXIT));
    Location exitLoc = ignoreToken();
    IdentifierInfo *name = 0;
    Location nameLoc = 0;

    if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        nameLoc = currentLocation();
        name = parseIdentifier();
    }

    Node condition = getNullNode();
    if (reduceToken(Lexer::TKN_WHEN)) {
        condition = parseExpr();
        if (condition.isInvalid()) {
            seekSemi();
            return getInvalidNode();
        }
    }

    return client.acceptExitStmt(exitLoc, name, nameLoc, condition);
}

Node Parser::parsePragmaStmt()
{
    assert(currentTokenIs(Lexer::TKN_PRAGMA));
    ignoreToken();

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifier();

    if (!name) {
        seekSemi();
        return getInvalidNode();
    }

    llvm::StringRef ref(name->getString());
    pragma::PragmaID ID = pragma::getPragmaID(ref);

    if (ID == pragma::UNKNOWN_PRAGMA) {
        report(loc, diag::UNKNOWN_PRAGMA) << name;
        seekSemi();
        return getInvalidNode();
    }

    // Currently, the only pragma accepted in a statement context is Assert.
    // When the set of valid pragmas expands, special parsers will be written to
    // parse the arguments.
    switch (ID) {
    default:
        seekSemi();
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
        if (condition.isInvalid()) {
            seekToken(Lexer::TKN_RPAREN);
            return getInvalidNode();
        }
        args.push_back(condition);

        if (reduceToken(Lexer::TKN_COMMA)) {
            Node message = parseExpr();
            if (message.isInvalid()) {
                seekToken(Lexer::TKN_RPAREN);
                return getInvalidNode();
            }
            args.push_back(message);
        }

        if (!requireToken(Lexer::TKN_RPAREN)) {
            seekToken(Lexer::TKN_RPAREN);
            return getInvalidNode();
        }
        else
            return client.acceptPragmaStmt(name, loc, args);
    }
    seekSemi();
    return getInvalidNode();
}

Node Parser::parseRaiseStmt()
{
    assert(currentTokenIs(Lexer::TKN_RAISE));
    Location raiseLoc = ignoreToken();

    Node exception = parseName();
    if (exception.isInvalid()) {
        seekSemi();
        return getInvalidNode();
    }

    Node message = getNullNode();
    if (reduceToken(Lexer::TKN_WITH)) {
        message = parseExpr();
        if (message.isInvalid()) {
            seekSemi();
            return getInvalidNode();
        }
    }
    return client.acceptRaiseStmt(raiseLoc, exception, message);
}

void Parser::parseExceptionStmt(Node context)
{
    assert(currentTokenIs(Lexer::TKN_EXCEPTION));
    ignoreToken();

    // FIXME: Recovery from parse errors needs to be improved considerably.
    bool seenOthers = false;
    Location othersLoc = 0;
    do {
        Location loc = currentLocation();
        if (!requireToken(Lexer::TKN_WHEN)) {
            seekToken(Lexer::TKN_END);
            return;
        }

        if (seenOthers) {
            report(othersLoc, diag::OTHERS_HANDLER_NOT_FINAL);
            seekToken(Lexer::TKN_END);
            return;
        }

        // FIXME: Exception occurrence declarations are not yet supported, just
        // choices.
        NodeVector choices;
        do {
            Location choiceLoc = currentLocation();
            if (reduceToken(Lexer::TKN_OTHERS)) {
                if (!choices.empty()) {
                    report(choiceLoc, diag::OTHERS_HANDLER_NOT_UNIQUE);
                    seekToken(Lexer::TKN_END);
                    return;
                }
                seenOthers = true;
                othersLoc = choiceLoc;
                break;
            }

            Node exception = parseName();
            if (exception.isValid())
                choices.push_back(exception);
            else {
                seekToken(Lexer::TKN_END);
                return;
            }
        } while (reduceToken(Lexer::TKN_BAR));

        if (!requireToken(Lexer::TKN_RDARROW)) {
            seekToken(Lexer::TKN_END);
            return;
        }

        Node handler = client.beginHandlerStmt(loc, choices);
        if (handler.isInvalid()) {
            seekToken(Lexer::TKN_END);
            return;
        }

        do {
            Node stmt = parseStatement();
            if (stmt.isValid())
                client.acceptStmt(handler, stmt);
        } while (!currentTokenIs(Lexer::TKN_WHEN) &&
                 !currentTokenIs(Lexer::TKN_END) &&
                 !currentTokenIs(Lexer::TKN_EOT));

        client.endHandlerStmt(context, handler);
    } while (currentTokenIs(Lexer::TKN_WHEN));
}
