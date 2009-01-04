//===-- ast/AstRewriter.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTREWRITER_HDR_GUARD
#define COMMA_AST_ASTREWRITER_HDR_GUARD

#include "comma/ast/AstBase.h"
#include <map>

namespace comma {

class AstRewriter {

public:
    void addRewrite(ModelType *source, ModelType *target) {
        rewrites[source] = target;
    }

    SignatureType *rewrite(SignatureType *sig);
    DomainType *rewrite(DomainType *dom);

private:

    std::map<void *, void *> rewrites;
};


} // End comma namespace

#endif
