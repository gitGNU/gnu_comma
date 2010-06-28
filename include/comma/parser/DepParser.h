//===-- parser/DepParser.h ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_DEPPARSER_HDR_GUARD
#define COMMA_PARSER_DEPPARSER_HDR_GUARD

#include "comma/parser/ParserBase.h"

#include <set>

namespace comma {

/// \class
/// \brief Parses a Comma source file to determine the top-level entity and
///  dependencies.
///
class DepParser : public ParserBase {

public:
    DepParser(TextProvider   &txtProvider,
              IdentifierPool &idPool,
              Diagnostic     &diag)
        : ParserBase(txtProvider, idPool, diag) { }

    typedef std::pair<Location, std::string> DepEntry;
    typedef std::set<DepEntry> DepSet;

    /// Processes all 'with' clauses, populating the given set with the
    /// dependencies represented as strings.  Returns true if the parse was
    /// successful and false otherwise.
    bool parseDependencies(DepSet &dependencies);

private:
    /// Skips past a 'use' clause.  Returns true if the clause was successfully
    /// skipped and false otherwise.
    bool skipUse();

    /// Parses a with clause and generates the associated dependency info.
    /// Returns true if the parse was successful and false otherwise.  Adds the
    /// parsed dependency into the given set.
    bool parseWithClause(DepSet &dependencies);
};

} // end comma namespace.

#endif
