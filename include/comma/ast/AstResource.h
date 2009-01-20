//===-- ast/AstResource.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// The AstResource class provides access to "universal" resources necessary for
// builing and managing Ast trees.  It provides mechanisms for managing the
// allocation of nodes, a single point of contact for fundamental facilities
// like text providers and an identifier pool.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTRESOURCE_HDR_GUARD
#define COMMA_AST_ASTRESOURCE_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/basic/TextProvider.h"
#include "comma/ast/AstBase.h"

namespace comma {

class AstResource {

public:
    // For now, this class is simply a bag of important classes: A TextProvider
    // associated with the file being processed, and an IdentifierPool for the
    // global managment of identifiers.
    AstResource(TextProvider   &txtProvider,
                IdentifierPool &idPool);

    // FIXME: Eventually we will replace this single TextProvider resource with
    // a manager class which provides services to handle multiple input files.
    TextProvider &getTextProvider() { return txtProvider; }

    IdentifierPool &getIdentifierPool() { return idPool; }

private:
    TextProvider   &txtProvider;
    IdentifierPool &idPool;
};

} // End comma namespace.

#endif
