//===-- CheckSubroutine.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Type check logic supporting subroutine declarations and definitions.
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "Stencil.h"
#include "TypeCheck.h"
#include "comma/ast/AstRewriter.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"
#include "comma/basic/PrimitiveOps.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

void TypeCheck::beginFunctionDeclaration(IdentifierInfo *name, Location loc)
{
    routineStencil.init(name, loc, SRDeclStencil::FUNCTION_Stencil);
}

void TypeCheck::beginProcedureDeclaration(IdentifierInfo *name, Location loc)
{
    routineStencil.init(name, loc, SRDeclStencil::PROCEDURE_Stencil);
}

void TypeCheck::acceptSubroutineParameter(IdentifierInfo *formal, Location loc,
                                          Node declNode, PM::ParameterMode mode)
{
    TypeDecl *tyDecl = ensureCompleteTypeDecl(declNode);

    if (!tyDecl) {
        routineStencil.markInvalid();
        return;
    }

    // If we are building a function declaration, ensure that the parameter is
    // of mode "in".
    if (routineStencil.denotesFunction() &&
        (mode != PM::MODE_IN) && (mode != PM::MODE_DEFAULT)) {
        report(loc, diag::OUT_MODE_IN_FUNCTION);
        routineStencil.markInvalid();
        return;
    }

    // Check that this parameters name does not conflict with any previous
    // parameters.
    typedef SRDeclStencil::param_iterator iterator;
    for (iterator I = routineStencil.begin_params();
         I != routineStencil.end_params(); ++I) {
        ParamValueDecl *previousParam = *I;
        if (previousParam->getIdInfo() == formal) {
            report(loc, diag::DUPLICATE_FORMAL_PARAM) << formal;
            routineStencil.markInvalid();
            return;
        }
    }

    // Check that the parameter name does not conflict with the subroutine
    // declaration itself.
    if (formal == routineStencil.getIdInfo()) {
        report(loc, diag::CONFLICTING_DECLARATION)
            << formal << getSourceLoc(routineStencil.getLocation());
        routineStencil.markInvalid();
        return;
    }

    Type *paramTy = tyDecl->getType();
    ParamValueDecl *paramDecl = new ParamValueDecl(formal, paramTy, mode, loc);
    routineStencil.addParameter(paramDecl);
}

void TypeCheck::acceptFunctionReturnType(Node typeNode)
{
    assert(routineStencil.denotesFunction() &&
           "Inconsitent state for function returns!");

    if (typeNode.isNull()) {
        routineStencil.markInvalid();
        return;
    }

    TypeDecl *returnDecl = ensureCompleteTypeDecl(typeNode);
    if (!returnDecl) {
        routineStencil.markInvalid();
        return;
    }

    routineStencil.setReturnType(returnDecl);
}

Node TypeCheck::endSubroutineDeclaration(bool definitionFollows)
{
    IdentifierInfo *name = routineStencil.getIdInfo();
    Location location = routineStencil.getLocation();
    SRDeclStencil::ParamVec &params = routineStencil.getParams();

    // Ensure the stencil is reset once this method returns.
    ASTStencilReseter reseter(routineStencil);

    // If the subroutine stencil has not checked out thus far, do not construct
    // a subroutine declaration for it.
    if (routineStencil.isInvalid())
        return getInvalidNode();

    // Ensure that if this function names a binary or unary operator it has the
    // required arity.
    if (routineStencil.denotesFunction()) {
        bool namesUnary = PO::denotesUnaryOp(name);
        bool namesBinary = PO::denotesBinaryOp(name);

        if (namesUnary || namesBinary) {
            bool allOK = true;
            unsigned numParams = params.size();

            if (namesUnary && namesBinary)
                allOK = numParams == 1 || numParams == 2;
            else if (namesUnary)
                allOK = numParams == 1;
            else if (namesBinary)
                allOK = numParams == 2;

            if (!allOK) {
                report(location, diag::OPERATOR_ARITY_MISMATCH) << name;
                return getInvalidNode();
            }
        }
    }

    SubroutineDecl *routineDecl = 0;
    DeclRegion *region = currentDeclarativeRegion();
    if (routineStencil.denotesFunction()) {
        Type *returnType = routineStencil.getReturnType()->getType();
        routineDecl = new FunctionDecl(resource, name, location,
                                       params.data(), params.size(),
                                       returnType, region);
    }
    else
        routineDecl = new ProcedureDecl(resource, name, location,
                                        params.data(), params.size(),
                                        region);

    // Ensure this new declaration does not conflict with any other currently in
    // scope.
    if (Decl *conflict = scope.addDirectDecl(routineDecl)) {
        // If the conflict is a subroutine, check if the current declaration can
        // serve as a completion.
        SubroutineDecl *fwdDecl = dyn_cast<SubroutineDecl>(conflict);
        if (fwdDecl && definitionFollows &&
            compatibleSubroutineDecls(fwdDecl, routineDecl)) {

            // If the conflict does not already have a completion link in this
            // routine.
            if (fwdDecl->hasDefiningDeclaration()) {
                report(location, diag::SUBROUTINE_REDECLARATION)
                    << fwdDecl->getIdInfo()
                    << getSourceLoc(fwdDecl->getLocation());
                return getInvalidNode();
            }

            fwdDecl->setDefiningDeclaration(routineDecl);

            // If the forward declaration is present in the current region we do
            // not add this node to the region.  This ensures that only one
            // subroutine declaration with a given profile is listed.
            if (!fwdDecl->isDeclaredIn(region))
                region->addDecl(routineDecl);
        }
        else {
            report(location, diag::CONFLICTING_DECLARATION)
                << name << getSourceLoc(conflict->getLocation());
            return getInvalidNode();
        }
    }
    else {
        // The subroutine does not conflict or serve as a completion.  Add it to
        // the current declarative region.
        region->addDecl(routineDecl);
    }

    // Since the declaration has been added permanently to the environment,
    // ensure the returned Node does not reclaim the decl.
    Node routine = getNode(routineDecl);
    routine.release();
    return routine;
}

Node TypeCheck::beginSubroutineDefinition(Node declarationNode)
{
    SubroutineDecl *srDecl = cast_node<SubroutineDecl>(declarationNode);
    declarationNode.release();

    // Enter a scope for the subroutine definition.  Add the subroutine itself
    // as an element of the new scope and add the formal parameters.  This
    // should never result in conflicts.
    scope.push(SUBROUTINE_SCOPE);
    scope.addDirectDeclNoConflicts(srDecl);
    typedef SubroutineDecl::param_iterator param_iterator;
    for (param_iterator I = srDecl->begin_params();
         I != srDecl->end_params(); ++I)
        scope.addDirectDeclNoConflicts(*I);

    // Allocate a BlockStmt for the subroutines body and make this block the
    // current declarative region.
    assert(!srDecl->hasBody() && "Current subroutine already has a body!");
    BlockStmt *block = new BlockStmt(0, srDecl, srDecl->getIdInfo());
    srDecl->setBody(block);
    declarativeRegion = block;
    return declarationNode;
}

void TypeCheck::endSubroutineDefinition()
{
    assert(scope.getKind() == SUBROUTINE_SCOPE);

    // We established two levels of declarative regions in
    // beginSubroutineDefinition: one for the BlockStmt constituting the body
    // and another corresponding to the subroutine itself.  Pop them both.
    declarativeRegion = declarativeRegion->getParent()->getParent();
    scope.pop();
}

/// Returns true if the given parameter is of mode "in", and thus capatable with
/// a function declaration.  Otherwise false is returned an a diagnostic is
/// posted.
bool TypeCheck::checkFunctionParameter(ParamValueDecl *param)
{
    PM::ParameterMode mode = param->getParameterMode();
    if (mode == PM::MODE_IN)
        return true;
    report(param->getLocation(), diag::OUT_MODE_IN_FUNCTION);
    return false;
}

bool
TypeCheck::compatibleSubroutineDecls(SubroutineDecl *X, SubroutineDecl *Y)
{
    if (X->getIdInfo() != Y->getIdInfo())
        return false;

    if (X->getType() != Y->getType())
        return false;

    if (!X->paramModesMatch(Y))
        return false;

    if (!X->keywordsMatch(Y))
        return false;

    return true;
}
