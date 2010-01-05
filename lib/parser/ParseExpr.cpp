//===-- parser/parser.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"

#include "llvm/ADT/APInt.h"

#include <cassert>

using namespace comma;

Node Parser::parseExpr()
{
    return parseOperatorExpr();
}

Node Parser::parseOperatorExpr()
{
    Location loc;
    IdentifierInfo *opInfo;

    Node lhs = parseRelationalOperator();

    if (lhs.isInvalid())
        return getInvalidNode();

    switch (currentTokenCode()) {

    default:
        return lhs;

    case Lexer::TKN_AND:
    case Lexer::TKN_OR:
    case Lexer::TKN_XOR:
        loc = currentLocation();
        opInfo = parseFunctionIdentifier();
        break;
    }

    Node rhs = parseRelationalOperator();

    if (rhs.isValid()) {
        Node prefix = client.acceptDirectName(opInfo, loc, false);
        if (prefix.isValid()) {
            NodeVector args;
            args.push_back(lhs);
            args.push_back(rhs);
            return client.acceptApplication(prefix, args);
        }
    }
    return getInvalidNode();
}

Node Parser::parseExponentialOperator()
{
    IdentifierInfo *opInfo;
    Location loc;

    if (currentTokenIs(Lexer::TKN_NOT)) {
        loc = currentLocation();
        opInfo = parseFunctionIdentifier();

        Node operand = parsePrimaryExpr();

        if (operand.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(operand);
                return client.acceptApplication(prefix, args);
            }
        }
        return getInvalidNode();
    }

    Node lhs = parsePrimaryExpr();

    if (lhs.isInvalid())
        return getInvalidNode();

    switch (currentTokenCode()) {

    default:
        return lhs;

    case Lexer::TKN_POW:
        loc    = currentLocation();
        opInfo = parseFunctionIdentifier();
        break;
    }

    // Right associativity for the exponential operator, hense the recursive
    // call.
    Node rhs = parseExponentialOperator();

    if (rhs.isValid()) {
        Node prefix = client.acceptDirectName(opInfo, loc, false);
        if (prefix.isValid()) {
            NodeVector args;
            args.push_back(lhs);
            args.push_back(rhs);
            return client.acceptApplication(prefix, args);
        }
    }
    return getInvalidNode();
}

Node Parser::parseMultiplicativeOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parseExponentialOperator();

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_STAR:
        case Lexer::TKN_FSLASH:
        case Lexer::TKN_MOD:
        case Lexer::TKN_REM:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifier();
            break;
        }

        Node rhs = parseExponentialOperator();

        if (rhs.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptApplication(prefix, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseAdditiveOperator()
{
    Node lhs = getNullNode();

    if (currentTokenIs(Lexer::TKN_PLUS) || currentTokenIs(Lexer::TKN_MINUS)) {
        Location loc = currentLocation();
        IdentifierInfo *opInfo = parseFunctionIdentifier();

        lhs = parseMultiplicativeOperator();

        if (!lhs.isValid())
            return getInvalidNode();

        Node prefix = client.acceptDirectName(opInfo, loc, false);
        if (prefix.isValid()) {
            NodeVector args;
            args.push_back(lhs);
            lhs = client.acceptApplication(prefix, args);
        }
    }
    else
        lhs = parseMultiplicativeOperator();

    return parseBinaryAdditiveOperator(lhs);
}

Node Parser::parseBinaryAdditiveOperator(Node lhs)
{
    IdentifierInfo *opInfo;
    Location loc;

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_PLUS:
        case Lexer::TKN_MINUS:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifier();
            break;
        }

        Node rhs = parseMultiplicativeOperator();

        if (rhs.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptApplication(prefix, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseRelationalOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parseAdditiveOperator();

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_EQUAL:
        case Lexer::TKN_NEQUAL:
        case Lexer::TKN_LESS:
        case Lexer::TKN_GREAT:
        case Lexer::TKN_LEQ:
        case Lexer::TKN_GEQ:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifier();
            break;
        }

        Node rhs = parseAdditiveOperator();

        if (rhs.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptApplication(prefix, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseParenExpr()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    if (aggregateFollows())
        return parseAggregate();
    else {
        ignoreToken();          // Consume the opening paren.
        Node result = parseExpr();
        if (!reduceToken(Lexer::TKN_RPAREN))
            report(diag::UNEXPECTED_TOKEN_WANTED) <<
                currentTokenString() << ")";
        return result;
    }
}

Node Parser::parsePrimaryExpr()
{
    switch (currentTokenCode()) {

    default:
        return parseName();

    case Lexer::TKN_LPAREN:
        return parseParenExpr();

    case Lexer::TKN_INTEGER:
        return parseIntegerLiteral();

    case Lexer::TKN_STRING:
        return parseStringLiteral();
    }
}

Node Parser::parseIntegerLiteral()
{
    assert(currentTokenIs(Lexer::TKN_INTEGER));

    const char *rep = currentToken().getRep();
    unsigned repLen = currentToken().getLength();
    Location loc = ignoreToken();

    llvm::APInt value;
    decimalLiteralToAPInt(rep, repLen, value);
    return client.acceptIntegerLiteral(value, loc);
}

Node Parser::parseStringLiteral()
{
    assert(currentTokenIs(Lexer::TKN_STRING));

    const char *rep = currentToken().getRep();
    unsigned repLen = currentToken().getLength();
    Location loc = ignoreToken();

    return client.acceptStringLiteral(rep, repLen, loc);
}

Node Parser::parseOthersExpr()
{
    assert(currentTokenIs(Lexer::TKN_OTHERS));
    Location loc = ignoreToken();

    if (!requireToken(Lexer::TKN_RDARROW))
        return getInvalidNode();

    // Return a null node when we hit a TKN_DIAMOND, otherwise parse the
    // expression.
    Node result = getNullNode();
    if (!reduceToken(Lexer::TKN_DIAMOND))
        result = parseExpr();

    // Diagnose that an others expression must come last if anything but a
    // closing paren is next on the stream.
    if (!currentTokenIs(Lexer::TKN_RPAREN)) {
        report(loc, diag::OTHERS_COMPONENT_NOT_FINAL);
        return getInvalidNode();
    }

    return result;
}

bool Parser::parseAggregateComponent(bool &seenKeyedComponent)
{
    NodeVector keys;

    do {
        Location loc = currentLocation();

        // Check for the special case of an identifier followed by a `=>' or
        // `|'.
        if (currentTokenIs(Lexer::TKN_IDENTIFIER) &&
            (nextTokenIs(Lexer::TKN_RDARROW) || nextTokenIs(Lexer::TKN_BAR))) {
            IdentifierInfo *name = parseIdentifier();
            Node key = client.acceptAggregateKey(name, loc);
            if (key.isValid())
                keys.push_back(key);
            continue;
        }

        Node lower = parseExpr();
        if (lower.isInvalid()) {
            seekTokens(Lexer::TKN_BAR, Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
            continue;
        }

        if (currentTokenIs(Lexer::TKN_COMMA) ||
            currentTokenIs(Lexer::TKN_RPAREN)) {
            if (seenKeyedComponent) {
                report(loc, diag::POSITIONAL_FOLLOWING_KEYED_COMPONENT);
                seekCloseParen();
                return false;
            }
            client.acceptPositionalAggregateComponent(lower);
            return true;
        }

        if (reduceToken(Lexer::TKN_DDOT)) {
            Node upper = parseExpr();
            if (upper.isInvalid())
                seekTokens(Lexer::TKN_BAR, Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
            else {
                Node key = client.acceptAggregateKey(lower, upper);
                if (key.isValid())
                    keys.push_back(key);
            }
        }
        else {
            Node key = client.acceptAggregateKey(lower);
            if (key.isValid())
                keys.push_back(key);
        }
    } while (reduceToken(Lexer::TKN_BAR));

    seenKeyedComponent = true;
    if (requireToken(Lexer::TKN_RDARROW)) {
        Node expr = parseExpr();
        if (expr.isValid())
            client.acceptKeyedAggregateComponent(keys, expr);
    }
    return true;
}

Node Parser::parseAggregate()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    client.beginAggregate(ignoreToken());
    bool seenKeyedComponent = false;

    // Parse each component.
    do {
        if (currentTokenIs(Lexer::TKN_OTHERS)) {
            Location loc = currentLocation();
            Node others = parseOthersExpr();

            if (others.isValid()) {
                client.acceptAggregateOthers(loc, others);
                requireToken(Lexer::TKN_RPAREN);
            }
            else
                seekCloseParen();
            return client.endAggregate();
        }

        if (!parseAggregateComponent(seenKeyedComponent))
            return getInvalidNode();

    } while (reduceToken(Lexer::TKN_COMMA));

    requireToken(Lexer::TKN_RPAREN);
    return client.endAggregate();
}

