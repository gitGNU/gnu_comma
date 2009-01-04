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
    declarations.reserve(32);
}

Scope::Scope(ScopeKind kind, Scope *parent)
    : kind(kind),
      parentScope(parent),
      childScope(0)
{
    assert(parent->childScope == 0);
    declarations.reserve(32);
    parentScope->childScope = this;
}

// This function ensures that there are no duplicate decls living in the
// declarations array.  The body of this function, in time, should probably be
// conditional on a COMMA_DEBUG define or some such.
void Scope::ensureDisjointDeclarations() const
{
    for (unsigned i = 0; i < declarations.size() - 1; ++i)
        for (unsigned j = i + 1; j < declarations.size(); ++j)
            assert(declarations[i] != declarations[j] &&
                   "Duplicate declarations found in same scope!");
}

Scope *Scope::pushScope(ScopeKind kind)
{
    // FIXME: Once all of the scope kinds are nailed down a few assertions
    // should be implemented insuring we are not pushing scopes in contexts
    // which do not make sense.
    return new Scope(kind, this);
}

void Scope::modifyDeclStack(IdentifierInfo *idInfo, ModelDecl *decl)
{
    DeclStack *declStack = idInfo->getMetadata<DeclStack>();

    if (declStack) {
        if (declStack->back().scope != this) {
            declStack->push_back(DeclInfo(this));
        }
        else {
            assert(declStack->back().type == 0 &&
                   "Attempt to add multiple type declarations!");
        }
    }
    else {
        declStack = new DeclStack();
        declStack->reserve(4);
        declStack->push_back(DeclInfo(this));
        idInfo->setMetadata(declStack);
    }
    DeclInfo *declInfo = &declStack->back();
    declInfo->type = decl;
}

void Scope::addModel(IdentifierInfo *idInfo, ModelDecl *decl)
{
    modifyDeclStack(idInfo, decl);

    // Add the declaration to the set we are responsible for.
    declarations.push_back(decl);
    ensureDisjointDeclarations();
}

void Scope::addModel(ModelDecl *decl)
{
    addModel(decl->getIdInfo(), decl);
}

ModelDecl *Scope::lookupModel(const IdentifierInfo *info, bool traverse) const
{
    if (info->hasMetadata()) {
        DeclStack *stack = info->getMetadata<DeclStack>();
        DeclInfo *declInfo = &stack->back();

        if (declInfo->type != 0)
            return llvm::dyn_cast<ModelDecl>(declInfo->type);

        if (traverse) {
            // Ascend the chain of parents, returning the first declaration
            // found.
            DeclStack::iterator iter = stack->end();
            DeclStack::iterator endIter = stack->begin();
            for (--iter; iter != endIter; --iter) {
                if (iter->type != 0)
                    return llvm::dyn_cast<ModelDecl>(iter->type);
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
    std::vector<Decl *>::iterator iter;
    std::vector<Decl *>::iterator endIter = declarations.end();
    for (iter = declarations.begin(); iter != endIter; ++iter) {
        IdentifierInfo *info = (*iter)->getIdInfo();

        DeclStack *declStack = info->getMetadata<DeclStack>();
        assert(declStack && "IdInfo metadata corruption?");

        DeclInfo *declInfo = &declStack->back();
        assert(declInfo->scope == this && "Decl stack corruption?");

        declStack->pop_back();
    }

    // Unlink this scope from the parent.
    parentScope->childScope = 0;
    return parentScope;
}
