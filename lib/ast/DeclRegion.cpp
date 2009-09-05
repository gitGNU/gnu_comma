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
#include "comma/ast/Type.h"

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

    case Ast::AST_FunctionDecl:
        newDecl = rewriteFunctionDecl(cast<FunctionDecl>(decl), rewrites);
        break;

    case Ast::AST_ProcedureDecl:
        newDecl = rewriteProcedureDecl(cast<ProcedureDecl>(decl), rewrites);
        break;

    case Ast::AST_EnumerationDecl:
        // Nothing to do for an enumeration since there are never free variables
        // in such a type.
        newDecl = decl;
        break;
    }

    if (newDecl)
        this->addDecl(newDecl);
}

FunctionDecl *DeclRegion::rewriteFunctionDecl(FunctionDecl *fdecl,
                                              const AstRewriter &rewrites)
{
    llvm::SmallVector<ParamValueDecl*, 8> params;
    unsigned arity = fdecl->getArity();

    for (unsigned i = 0; i < arity; ++i) {
        ParamValueDecl *origParam = fdecl->getParam(i);
        Type *rewriteType = rewrites.rewrite(origParam->getType());
        ParamValueDecl *newParam =
            new ParamValueDecl(origParam->getIdInfo(), rewriteType,
                               origParam->getExplicitParameterMode(), 0);
        params.push_back(newParam);
    }

    FunctionDecl *result =
        new FunctionDecl(rewrites.getAstResource(),
                         fdecl->getIdInfo(), 0, params.data(), arity,
                         rewrites.rewrite(fdecl->getReturnType()), this);
    result->setOrigin(fdecl);
    return result;
}

ProcedureDecl *DeclRegion::rewriteProcedureDecl(ProcedureDecl *pdecl,
                                                const AstRewriter &rewrites)
{
    llvm::SmallVector<ParamValueDecl*, 8> params;
    unsigned arity = pdecl->getArity();

    for (unsigned i = 0; i < arity; ++i) {
        ParamValueDecl *origParam = pdecl->getParam(i);
        Type *newType = rewrites.rewrite(origParam->getType());
        ParamValueDecl *newParam =
            new ParamValueDecl(origParam->getIdInfo(), newType,
                               origParam->getExplicitParameterMode(), 0);
        params.push_back(newParam);
    }

    ProcedureDecl *result =
        new ProcedureDecl(rewrites.getAstResource(),
                          pdecl->getIdInfo(), 0, params.data(), arity, this);
    result->setOrigin(pdecl);
    return result;
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
            Type *candidate = 0;
            if (TypeDecl *TD = dyn_cast<TypeDecl>(decl))
                candidate = TD->getType();
            else if (SubroutineDecl *SD = dyn_cast<SubroutineDecl>(decl))
                candidate = SD->getType();
            else if (EnumLiteral *EL = dyn_cast<EnumLiteral>(decl))
                candidate = EL->getType();

            if (candidate && type->equals(candidate))
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
    case Ast::AST_PercentDecl:
        return static_cast<const PercentDecl*>(this);
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
    case Ast::AST_IntegerDecl:
        return static_cast<const IntegerDecl*>(this);
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
