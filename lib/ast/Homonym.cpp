//===-- ast/Homonym.cpp --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Homonym.h"

using namespace comma;


Homonym::VisibilitySet *Homonym::getOrCreateVisibilitySet()
{
    if (!isLoaded()) {

        VisibilitySet *set = new VisibilitySet();
        Decl  *currentDecl;

        switch (rep.getInt()) {

        default:
            assert(false && "Bad type of homonym!");
            break;

        case HOMONYM_DIRECT:
            currentDecl = asDeclaration();
            set->directDecls.push_back(currentDecl);
            break;

        case HOMONYM_IMPORT:
            currentDecl = asDeclaration();
            set->importDecls.push_back(currentDecl);
            break;

        case HOMONYM_EMPTY:
            break;
        }
        rep.setPointer(set);
        rep.setInt(HOMONYM_LOADED);
    }
    return asVisibilitySet();
}
