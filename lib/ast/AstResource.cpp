//===-- ast/AstResource.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/Type.h"
#include "comma/ast/Decl.h"

using namespace comma;

AstResource::AstResource(TextProvider   &txtProvider,
                         IdentifierPool &idPool)
    : txtProvider(txtProvider),
      idPool(idPool) { }
