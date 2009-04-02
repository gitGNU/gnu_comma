//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"
#include "comma/typecheck/TypeCheck.h"
#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

Node TypeCheck::acceptProcedureCall(IdentifierInfo  *name,
                                    Location         loc,
                                    Node            *args,
                                    unsigned         numArgs)
{
    return acceptSubroutineCall(name, loc, args, numArgs, false);
}


Node TypeCheck::acceptReturnStmt(Location loc, Node retNode)
{
    assert((checkingProcedure() || checkingFunction()) &&
           "Return statement outside subroutine context!");

    if (retNode.isNull()) {
        if (checkingProcedure())
            return Node(new ReturnStmt(loc));

        report(loc, diag::EMPTY_RETURN_IN_FUNCTION);
        return Node::getInvalidNode();
    }
    else {
        if (checkingFunction()) {
            FunctionDecl *fdecl      = getCurrentFunction();
            Expr         *retExpr    = cast_node<Expr>(retNode);
            Type         *targetType = fdecl->getReturnType();

            if (retExpr->hasType()) {
                if (targetType->equals(retExpr->getType()))
                    return Node(new ReturnStmt(loc, retExpr));
                report(loc, diag::INCOMPATABLE_TYPES);
                return Node::getInvalidNode();
            }

            FunctionCallExpr *fcall = cast<FunctionCallExpr>(retExpr);
            if (!resolveFunctionCall(fcall, targetType))
                return Node::getInvalidNode();

            return Node(new ReturnStmt(loc, retExpr));
        }

        report(loc, diag::NONEMPTY_RETURN_IN_PROCEDURE);
        return Node::getInvalidNode();
    }
}
