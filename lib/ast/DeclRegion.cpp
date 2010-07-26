//===-- ast/DeclRegion.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//


#include "comma/ast/AstResource.h"
#include "comma/ast/AstRewriter.h"
#include "comma/ast/Decl.h"
#include "comma/ast/DeclRegion.h"
#include "comma/ast/DeclRewriter.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"

#include "llvm/Support/Casting.h"

#include <algorithm>
#include <cstring>
#include <cassert>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void DeclRegion::addDecl(Decl *decl)
{
    declarations.push_back(decl);
    notifyObserversOfAddition(decl);
}

void DeclRegion::addDeclarationUsingRewrites(DeclRewriter &rewrites,
                                             Decl *decl)
{
    addDecl(rewrites.rewriteDecl(decl));
}

void
DeclRegion::addDeclarationsUsingRewrites(DeclRewriter &rewrites,
                                         const DeclRegion *region)
{
    ConstDeclIter E = region->endDecls();
    for (ConstDeclIter I = region->beginDecls(); I != E; ++I)
        addDecl(rewrites.rewriteDecl(*I));
}

Decl *DeclRegion::findDecl(IdentifierInfo *name, Type *type)
{
    DeclIter endIter = endDecls();
    for (DeclIter iter = beginDecls(); iter != endIter; ++iter) {
        Decl *decl = *iter;
        if (decl->getIdInfo() == name) {
            Type *candidate = 0;
            if (TypeDecl *TD = dyn_cast<TypeDecl>(decl))
                candidate = TD->getType();
            else if (SubroutineDecl *SD = dyn_cast<SubroutineDecl>(decl))
                candidate = SD->getType();
            else if (ValueDecl *VD = dyn_cast<ValueDecl>(decl))
                candidate = VD->getType();

            if (candidate && type == candidate)
                return decl;
        }
    }
    return 0;
}

DeclRegion::PredRange
DeclRegion::findDecls(IdentifierInfo *name) const
{
    struct Pred : public PredIter::Predicate {
        Pred(IdentifierInfo *name) : name(name) { }
        bool operator()(const Decl *decl) {
            return decl->getIdInfo() == name;
        }
        IdentifierInfo *name;
    };
    ConstDeclIter begin = beginDecls();
    ConstDeclIter end   = endDecls();
    Pred         *pred  = new Pred(name);
    return PredRange(PredIter(pred, begin, end), PredIter(end));
}

bool DeclRegion::removeDecl(Decl *decl)
{
    DeclIter result = std::find(beginDecls(), endDecls(), decl);
    if (result != endDecls()) {
        declarations.erase(result);
        return true;
    }
    return false;
}

bool DeclRegion::containsDecl(const Decl *decl) const
{
    ConstDeclIter I = beginDecls();
    ConstDeclIter E = endDecls();
    ConstDeclIter P = std::find(I, E, decl);
    return P != E;
}

bool DeclRegion::collectFunctionDecls(
    IdentifierInfo *name, llvm::SmallVectorImpl<SubroutineDecl*> &dst)
{
    PredRange range = findDecls(name);
    size_t size = dst.size();

    for (PredIter iter = range.first; iter != range.second; ++iter) {
        if (FunctionDecl *decl = dyn_cast<FunctionDecl>(*iter))
            dst.push_back(decl);
    }
    return size != dst.size();
}

bool DeclRegion::collectProcedureDecls(
    IdentifierInfo *name, llvm::SmallVectorImpl<SubroutineDecl*> &dst)
{
    PredRange range = findDecls(name);
    size_t size = dst.size();

    for (PredIter iter = range.first; iter != range.second; ++iter) {
        if (ProcedureDecl *decl = dyn_cast<ProcedureDecl>(*iter))
            dst.push_back(decl);
    }
    return size != dst.size();
}

const Ast *DeclRegion::asAst() const
{
    switch (regionKind) {
    default:
        assert(false && "Unknown delcarative region kind!");
        return 0;
    case Ast::AST_FunctionDecl:
        return static_cast<const FunctionDecl*>(this);
    case Ast::AST_ProcedureDecl:
        return static_cast<const ProcedureDecl*>(this);
    case Ast::AST_BodyDecl:
        return static_cast<const BodyDecl*>(this);
    case Ast::AST_PackageDecl:
        return static_cast<const PackageDecl*>(this);
    case Ast::AST_PrivatePart:
        return static_cast<const PrivatePart*>(this);
    case Ast::AST_PkgInstanceDecl:
        return static_cast<const PkgInstanceDecl*>(this);
    case Ast::AST_EnumerationDecl:
        return static_cast<const EnumerationDecl*>(this);
    case Ast::AST_BlockStmt:
        return static_cast<const BlockStmt*>(this);
    case Ast::AST_IntegerDecl:
        return static_cast<const IntegerDecl*>(this);
    case Ast::AST_RecordDecl:
        return static_cast<const RecordDecl*>(this);
    case Ast::AST_ArrayDecl:
        return static_cast<const ArrayDecl*>(this);
    case Ast::AST_AccessDecl:
        return static_cast<const AccessDecl*>(this);
    }
}

Ast *DeclRegion::asAst()
{
    return const_cast<Ast*>(
        const_cast<const DeclRegion *>(this)->asAst());
}

// Default implementation -- do nothing.
void DeclRegion::notifyAddDecl(Decl *decl) { }

// Default implementation -- do nothing.
void DeclRegion::notifyRemoveDecl(Decl *decl) { }

void DeclRegion::notifyObserversOfAddition(Decl *decl)
{
    for (ObserverList::iterator iter = observers.begin();
         iter != observers.end(); ++iter)
        (*iter)->notifyAddDecl(decl);
}

void DeclRegion::notifyObserversOfRemoval(Decl *decl)
{
    for (ObserverList::iterator iter = observers.begin();
         iter != observers.end(); ++iter)
        (*iter)->notifyRemoveDecl(decl);
}
