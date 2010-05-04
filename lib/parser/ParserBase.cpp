//===-- parser/ParserBase.cpp --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/ParserBase.h"
#include <algorithm>

using namespace comma;

ParserBase::ParserBase(TextProvider &txtProvider, IdentifierPool &idPool,
                       Diagnostic &diag)
    : txtProvider(txtProvider),
      diagnostic(diag),
      idPool(idPool),
      lexer(txtProvider, diag)
{
    // Prime the parser by loading in the first token.
    lexer.scan(token);
}

DiagnosticStream &ParserBase::report(diag::Kind kind)
{
    if (currentLocation().isValid()) {
        SourceLocation sloc = txtProvider.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }
    else
        return diagnostic.report(kind);
}

bool ParserBase::requireToken(Lexer::Code code)
{
    bool status = reduceToken(code);
    if (!status)
        report(diag::UNEXPECTED_TOKEN_WANTED)
            << currentToken().getString()
            << Lexer::tokenString(code);
    return status;
}

bool ParserBase::seekToken(Lexer::Code code)
{
    while (!currentTokenIs(Lexer::TKN_EOT)) {
        if (currentTokenIs(code))
            return true;
        else
            ignoreToken();
    }
    return false;
}

bool ParserBase::seekAndConsumeToken(Lexer::Code code)
{
    bool status = seekToken(code);
    if (status) ignoreToken();
    return status;
}

bool ParserBase::seekTokens(Lexer::Code code0, Lexer::Code code1,
                            Lexer::Code code2, Lexer::Code code3,
                            Lexer::Code code4, Lexer::Code code5)
{
    Lexer::Code codes[] = { code0, code1, code2, code3, code4, code5 };
    Lexer::Code *end = &codes[6];

    while (!currentTokenIs(Lexer::TKN_EOT))
    {
        if (end != std::find(codes, end, currentTokenCode()))
            return true;
        else
            ignoreToken();
    }
    return false;
}

bool ParserBase::seekAndConsumeTokens(Lexer::Code code0,
                                      Lexer::Code code1, Lexer::Code code2,
                                      Lexer::Code code3, Lexer::Code code4)
{
    bool status = seekTokens(code0, code1, code2, code3, code4);
    if (status) ignoreToken();
    return status;
}

void ParserBase::beginExcursion()
{
    savedTokens.push_back(currentToken());
    lexer.beginExcursion();
}

void ParserBase::endExcursion()
{

    setCurrentToken(savedTokens.back());
    savedTokens.pop_back();
    lexer.endExcursion();
}

void ParserBase::forgetExcursion()
{
    lexer.forgetExcursion();
}

IdentifierInfo *ParserBase::getIdentifierInfo(const Lexer::Token &tkn)
{
    const char *rep = tkn.getRep();
    unsigned length = tkn.getLength();
    IdentifierInfo *info = &idPool.getIdentifierInfo(rep, length);
    return info;
}

IdentifierInfo *ParserBase::parseIdentifier()
{
    IdentifierInfo *info;

    switch (currentTokenCode()) {
    case Lexer::TKN_IDENTIFIER:
        info = getIdentifierInfo(currentToken());
        ignoreToken();
        break;

    case Lexer::TKN_EOT:
        report(diag::PREMATURE_EOS);
        info = 0;
        break;

    default:
        report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
        info = 0;
    }
    return info;
}

IdentifierInfo *ParserBase::parseFunctionIdentifier()
{
    IdentifierInfo *info;

    if (Lexer::isFunctionGlyph(currentToken())) {
        const char *rep = Lexer::tokenString(currentTokenCode());
        info = &idPool.getIdentifierInfo(rep);
        ignoreToken();
    }
    else
        info = parseIdentifier();
    return info;
}
