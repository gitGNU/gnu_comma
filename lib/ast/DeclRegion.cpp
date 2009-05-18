//===-- ast/DeclRegion.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstRewriter.h"
#include "comma/ast/DeclRegion.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Stmt.h"
#include "llvm/Support/Casting.h"
#include <algorithm>
#include <cstring>
#include <cassert>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void DeclRegion::addDecl(Decl *decl) {
    declarations.push_back(decl);
    notifyObserversOfAddition(decl);
}

void DeclRegion::addDeclarationUsingRewrites(const AstRewriter &rewrites,
                                             Decl *decl)
{
    Decl *newDecl = 0;

    switch (decl->getKind()) {

    default:
        assert(false && "Bad type of declaration!");
        break;

    case Ast::AST_FunctionDecl: {
        FunctionDecl *fdecl = cast<FunctionDecl>(decl);
        FunctionType *ftype = rewrites.rewrite(fdecl->getType());
        newDecl = new FunctionDecl(decl->getIdInfo(), 0, ftype, this);
        break;
    }

    case Ast::AST_ProcedureDecl: {
        ProcedureDecl *pdecl = cast<ProcedureDecl>(decl);
        ProcedureType *ptype = rewrites.rewrite(pdecl->getType());
        newDecl = new ProcedureDecl(decl->getIdInfo(), 0, ptype, this);
        break;
    }

    case Ast::AST_EnumerationDecl:
        // Nothing to do for an enumeration since there are never free variables
        // in such a type.
        newDecl = decl;
        break;
    }
    if (newDecl)
        this->addDecl(newDecl);
}

void
DeclRegion::addDeclarationsUsingRewrites(const AstRewriter &rewrites,
                                         const DeclRegion *region)
{
    ConstDeclIter iter;
    ConstDeclIter endIter = region->endDecls();

    for (iter = region->beginDecls(); iter != endIter; ++iter)
        addDeclarationUsingRewrites(rewrites, *iter);
}

Decl *DeclRegion::findDecl(IdentifierInfo *name, Type *type)
{
    DeclIter endIter = endDecls();
    for (DeclIter iter = beginDecls(); iter != endIter; ++iter) {
        Decl *decl = *iter;
        if (decl->getIdInfo() == name) {
            Type *candidateType = 0;
            if (ModelDecl *model = dyn_cast<ModelDecl>(decl))
                candidateType = model->getType();
            else if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(decl))
                candidateType = srDecl->getType();
            else if (EnumLiteral *elit = dyn_cast<EnumLiteral>(decl))
                candidateType = elit->getType();
            if (type->equals(candidateType))
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

bool DeclRegion::collectFunctionDecls(IdentifierInfo *name,
                                      unsigned        arity,
                                      std::vector<SubroutineDecl*> &dst)
{
    PredRange range = findDecls(name);
    size_t    size  = dst.size();

    for (PredIter iter = range.first; iter != range.second; ++iter) {
        if (FunctionDecl *decl = dyn_cast<FunctionDecl>(*iter)) {
            if (decl->getArity() == arity)
                dst.push_back(decl);
        }
    }
    return size != dst.size();
}

bool DeclRegion::collectProcedureDecls(IdentifierInfo *name,
                                       unsigned        arity,
                                       std::vector<SubroutineDecl*> &dst)
{
    PredRange range = findDecls(name);
    size_t    size  = dst.size();

    for (PredIter iter = range.first; iter != range.second; ++iter) {
        if (ProcedureDecl *decl = dyn_cast<ProcedureDecl>(*iter)) {
            if (decl->getArity() == arity)
                dst.push_back(decl);
        }
    }
    return size != dst.size();
}

const Ast *DeclRegion::asAst() const
{
    switch (regionKind) {
    default:
        assert(false && "Unknown delcarative region kind!");
        return 0;
    case Ast::AST_SignatureDecl:
        return static_cast<const SignatureDecl*>(this);
    case Ast::AST_VarietyDecl:
        return static_cast<const VarietyDecl*>(this);
    case Ast::AST_DomainDecl:
        return static_cast<const DomainDecl*>(this);
    case Ast::AST_FunctorDecl:
        return static_cast<const FunctorDecl*>(this);
    case Ast::AST_FunctionDecl:
        return static_cast<const FunctionDecl*>(this);
    case Ast::AST_ProcedureDecl:
        return static_cast<const ProcedureDecl*>(this);
    case Ast::AST_AbstractDomainDecl:
        return static_cast<const AbstractDomainDecl*>(this);
    case Ast::AST_DomainInstanceDecl:
        return static_cast<const DomainInstanceDecl*>(this);
    case Ast::AST_AddDecl:
        return static_cast<const AddDecl*>(this);
    case Ast::AST_EnumerationDecl:
        return static_cast<const EnumerationDecl*>(this);
    case Ast::AST_BlockStmt:
        return static_cast<const BlockStmt*>(this);
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
