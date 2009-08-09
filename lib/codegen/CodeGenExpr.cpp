//===-- codegen/CodeGenExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


llvm::Value *CodeGenRoutine::emitExpr(Expr *expr)
{
    llvm::Value *val = 0;

    switch (expr->getKind()) {
    default:
        assert(false && "Cannot codegen expression!");
        val = 0;
        break;

    case Ast::AST_DeclRefExpr:
        val = emitDeclRefExpr(cast<DeclRefExpr>(expr));
        break;

    case Ast::AST_FunctionCallExpr:
        val = emitFunctionCall(cast<FunctionCallExpr>(expr));
        break;
    }

    return val;
}

llvm::Value *CodeGenRoutine::emitDeclRefExpr(DeclRefExpr *expr)
{
    llvm::Value *val = lookupDecl(expr->getDeclaration());
    assert(val && "DeclRef lookup failed!");
    return val;
}

llvm::Value *CodeGenRoutine::emitFunctionCall(FunctionCallExpr *expr)
{
    std::vector<llvm::Value *> args;
    for (unsigned i = 0; i < expr->getNumArgs(); ++i)
        args.push_back(emitExpr(expr->getArg(i)));

    FunctionDecl *decl = cast<FunctionDecl>(expr->getConnective());

    if (decl->isPrimitive())
        return emitPrimitiveCall(expr, args);
    else if (isLocalCall(expr)) {
        // Insert the implicit first parameter, which for a local call is the
        // percent handed to the current subroutine.
        args.insert(args.begin(), percent);

        llvm::Value  *func = CG.lookupGlobal(CodeGen::getLinkName(decl));
        assert(func && "function lookup failed!");
        return Builder.CreateCall(func, args.begin(), args.end());
    }
    else if (isDirectCall(expr)) {
        // Lookup the domain info structure for the connective.
        DomainInstanceDecl *instance;
        Domoid *target;

        instance = cast<DomainInstanceDecl>(decl->getDeclRegion());
        target = instance->getDefiningDecl();
        llvm::GlobalValue *capsuleInfo = CG.lookupCapsuleInfo(target);
        assert(capsuleInfo && "Could not resolve info for direct call!");

        // Register the domain of computation with the capsule context.  Using
        // the ID of the instance, index into percent to obtain the appropriate
        // domain_instance.
        unsigned instanceID = CGC.addCapsuleDependency(instance);
        args.insert(args.begin(), CRT.getLocalCapsule(Builder, percent, instanceID));

        llvm::Value *func = CG.lookupGlobal(CodeGen::getLinkName(decl));
        assert(func && "function lookup failed!");

        return Builder.CreateCall(func, args.begin(), args.end());
    }
    else {
        // We must have an abstract call.
        return CRT.genAbstractCall(Builder, percent, decl, args);
    }
}


llvm::Value *CodeGenRoutine::emitPrimitiveCall(FunctionCallExpr *expr,
                                               std::vector<llvm::Value *> &args)
{
    FunctionDecl *decl = cast<FunctionDecl>(expr->getConnective());

    switch (decl->getPrimitiveID()) {

    default:
        assert(false && "Cannot codegen primitive!");
        return 0;

    case PO::Equality:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateICmpEQ(args[0], args[1]);

    case PO::EnumFunction: {
        EnumLiteral *lit = cast<EnumLiteral>(decl);
        unsigned idx = lit->getIndex();
        const llvm::Type *ty = CGTypes.lowerType(lit->getReturnType());
        return llvm::ConstantInt::get(ty, idx);
    }
    };
}
