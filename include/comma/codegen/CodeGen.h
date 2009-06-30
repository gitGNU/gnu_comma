//===-- comma/CodeGen.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGEN_HDR_GUARD
#define COMMA_CODEGEN_CODEGEN_HDR_GUARD

#include "comma/ast/AstBase.h"

#include <string>

namespace comma {

class CodeGen {

public:
    static std::string getLinkName(const SubroutineDecl *sr);

private:
    static int getDeclIndex(const Decl *decl, const DeclRegion *region);

    static std::string getSubroutineName(const SubroutineDecl *srd);
};


}; // end comma namespace


#endif
