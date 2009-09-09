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
    switch (currentTokenCode()) {

    default:
        return parseOperatorExpr();

    case Lexer::TKN_INJ:
        return parseInjExpr();

    case Lexer::TKN_PRJ:
        return parsePrjExpr();
    }
}

Node Parser::parseSubroutineKeywordSelection()
{
    assert(keywordSelectionFollows());
    Location        loc  = currentLocation();
    IdentifierInfo *key  = parseIdentifierInfo();

    ignoreToken();              // consume the "=>".
    Node expr = parseExpr();

    if (expr.isInvalid())
        return getInvalidNode();
    else
        return client.acceptKeywordSelector(key, loc, expr, true);
}

Node Parser::parseInjExpr()
{
    assert(currentTokenIs(Lexer::TKN_INJ));

    Location loc  = ignoreToken();
    Node     expr = parseExpr();

    if (expr.isValid())
        return client.acceptInj(loc, expr);
    return getInvalidNode();
}

Node Parser::parsePrjExpr()
{
    assert(currentTokenIs(Lexer::TKN_PRJ));

    Location loc  = ignoreToken();
    Node     expr = parseExpr();

    if (expr.isValid())
        return client.acceptPrj(loc, expr);
    return getInvalidNode();
}

Node Parser::parseOperatorExpr()
{
    return parseRelationalOperator();
}

Node Parser::parseExponentialOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parsePrimaryExpr();

    if (lhs.isInvalid())
        return getInvalidNode();

    switch (currentTokenCode()) {

    default:
        return lhs;

    case Lexer::TKN_HAT:
        loc    = currentLocation();
        opInfo = parseFunctionIdentifierInfo();
        break;
    }

    // Right associativity for the exponential operator, hense the recursive
    // call.
    Node rhs = parseExponentialOperator();

    if (rhs.isValid()) {
        Node fname = client.acceptFunctionName(opInfo, loc, getNullNode());
        if (fname.isValid()) {
            NodeVector args;
            args.push_back(lhs);
            args.push_back(rhs);
            return client.acceptFunctionCall(fname, loc, args);
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
            loc    = currentLocation();
            opInfo = parseFunctionIdentifierInfo();
            break;
        }

        Node rhs = parseExponentialOperator();

        if (rhs.isValid()) {
            Node fname = client.acceptFunctionName(opInfo, loc, getNullNode());
            if (fname.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptFunctionCall(fname, loc, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseAdditiveOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parseMultiplicativeOperator();

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_PLUS:
        case Lexer::TKN_MINUS:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifierInfo();
            break;
        }

        Node rhs = parseMultiplicativeOperator();

        if (rhs.isValid()) {
            Node fname = client.acceptFunctionName(opInfo, loc, getNullNode());
            if (fname.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptFunctionCall(fname, loc, args);
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
        case Lexer::TKN_LESS:
        case Lexer::TKN_GREAT:
        case Lexer::TKN_LEQ:
        case Lexer::TKN_GEQ:
        case Lexer::TKN_DIAMOND:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifierInfo();
            break;
        }

        Node rhs = parseAdditiveOperator();

        if (rhs.isValid()) {
            Node fname = client.acceptFunctionName(opInfo, loc, getNullNode());
            if (fname.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptFunctionCall(fname, loc, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parsePrimaryExpr()
{
    if (reduceToken(Lexer::TKN_LPAREN)) {
        Node result = parseExpr();
        if (!reduceToken(Lexer::TKN_RPAREN))
            report(diag::UNEXPECTED_TOKEN_WANTED)
                << currentTokenString() << ")";
        return result;
    }

    if (currentTokenIs(Lexer::TKN_INTEGER))
        return parseIntegerLiteral();

    Node qual = getNullNode();
    if (qualifierFollows()) {
       qual = parseQualifier();
       if (qual.isInvalid())
           return getInvalidNode();
    }

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierOrCharacter();

    if (!name)
        return getInvalidNode();

    if (currentTokenIs(Lexer::TKN_LPAREN)) {
        NodeVector args;
        if (!parseSubroutineArgumentList(args))
            return getInvalidNode();

        Node connective = client.acceptFunctionName(name, loc, qual);

        if (connective.isValid())
            return client.acceptFunctionCall(connective, loc, args);
        else
            return getInvalidNode();
    }
    else
        return client.acceptDirectName(name, loc, qual);
}

Node Parser::parseIntegerLiteral()
{
    assert(currentTokenIs(Lexer::TKN_INTEGER));

    typedef std::char_traits<char> Traits;

    const char *rep = currentToken().getRep();
    unsigned repLen = currentToken().getLength();
    Location loc = ignoreToken();

    llvm::APInt value;
    decimalLiteralToAPInt(rep, repLen, value);
    return client.acceptIntegerLiteral(value, loc);
}


