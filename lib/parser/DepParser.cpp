//===-- parser/DepParser.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/IdentifierInfo.h"
#include "comma/parser/DepParser.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"


using namespace comma;

bool DepParser::skipUse()
{
    assert(currentTokenIs(Lexer::TKN_USE));

    // We do not need to parse the use clause.  Simply seek and consume the
    // terminating semicolon.
    return seekAndConsumeToken(Lexer::TKN_SEMI);
}

bool DepParser::parseWithClause(DepSet &dependencies)
{
    assert(currentTokenIs(Lexer::TKN_WITH));

    Location loc = ignoreToken();
    llvm::SmallVector<IdentifierInfo*, 8> ids;

    do {
        IdentifierInfo *idInfo = parseIdentifier();
        if (!idInfo)
            return false;
        ids.push_back(idInfo);
    } while (reduceToken(Lexer::TKN_DOT));

    if (!requireToken(Lexer::TKN_SEMI))
        return false;

    llvm::Twine dep(ids[0]->getString());
    for (unsigned i = 1; i < ids.size(); ++i) {
        dep.concat(".");
        dep.concat(ids[i]->getString());
    }

    dependencies.insert(dependencies.end(), DepEntry(loc, dep.str()));
    return true;
}

bool DepParser::parseDependencies(DepSet &dependencies)
{
    bool status = true;

    while (status) {
        switch (currentTokenCode()) {

        default:
            return true;

        case Lexer::TKN_USE:
            status = skipUse();
            break;

        case Lexer::TKN_WITH:
            status = parseWithClause(dependencies);
            break;
        }
    }

    return status;
}
