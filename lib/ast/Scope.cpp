//===-- ast/Scope.cpp ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Scope.h"
#include "comma/ast/Decl.h"
#include "comma/basic/IdentifierInfo.h"
#include "llvm/Support/Casting.h"

using namespace comma;

Scope::Scope()
    : kind(CUNIT_SCOPE),
      parentScope(0),
      childScope(0)
{
}

Scope::Scope(ScopeKind kind, Scope *parent)
    : kind(kind),
      parentScope(parent),
      childScope(0)
{
    assert(parent->childScope == 0);
    parentScope->childScope = this;
}

Scope *Scope::pushScope(ScopeKind kind)
{
    // FIXME: Once all of the scope kinds are nailed down a few assertions
    // should be implemented insuring we are not pushing scopes in contexts
    // which do not make sense.
    return new Scope(kind, this);
}

Scope::DeclInfo *Scope::lookupDeclInfo(IdentifierInfo *idInfo)
{
    DeclStack *declStack = idInfo->getMetadata<DeclStack>();

    if (declStack && declStack->back().scope != this)
        declStack->push_back(DeclInfo(this));
    else {
        declStack = new DeclStack();
        declStack->reserve(4);
        declStack->push_back(DeclInfo(this));
        idInfo->setMetadata(declStack);
    }
    return &declStack->back();
}

void Scope::addType(ModelType *type)
{
    IdentifierInfo *idInfo = type->getIdInfo();
    DeclInfo *declInfo = lookupDeclInfo(type->getIdInfo());

    declInfo->type = type;
    identifiers.insert(idInfo);
}

ModelType *Scope::lookupType(const IdentifierInfo *info, bool traverse) const
{
    if (info->hasMetadata()) {
        DeclStack *stack = info->getMetadata<DeclStack>();
        DeclInfo *declInfo = &stack->back();

        if (declInfo->scope != this && !traverse)
            return 0;

        if (declInfo->type)
            return declInfo->type;

        if (traverse) {
            // Ascend the chain of parents, returning the first declaration
            // found.
            DeclStack::iterator iter = stack->end();
            DeclStack::iterator endIter = stack->begin();
            for (--iter; iter != endIter; --iter) {
                if (iter->type != 0)
                    return iter->type;
            }
        }
        return 0;
    }
    return 0;
}

Scope *Scope::popScope()
{
    // Traverse the set of declarations we are responsible for and reduce their
    // declaration stacks appropriately.  We do a bit of checking here to ensure
    // the scope stack is well formed.
    IdInfoSet::iterator iter = identifiers.begin();
    IdInfoSet::iterator endIter = identifiers.end();
    for ( ; iter != endIter; ++iter) {
        IdentifierInfo *info = *iter;

        DeclStack *declStack = info->getMetadata<DeclStack>();
        assert(declStack && "IdInfo metadata corruption?");

        DeclInfo *declInfo = &declStack->back();
        assert(declInfo->scope == this && "Decl stack corruption?");

        declStack->pop_back();
        if (declStack->empty()) {
            delete declStack;
            info->setMetadata(0);
        }
    }

    // Unlink this scope from the parent.
    parentScope->childScope = 0;
    return parentScope;
}
