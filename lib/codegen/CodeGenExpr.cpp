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

    case Ast::AST_InjExpr:
        val = emitInjExpr(cast<InjExpr>(expr));
        break;

    case Ast::AST_PrjExpr:
        val = emitPrjExpr(cast<PrjExpr>(expr));
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
    FunctionDecl *fdecl = cast<FunctionDecl>(expr->getConnective());
    std::vector<llvm::Value *> args;

    for (unsigned i = 0; i < expr->getNumArgs(); ++i) {
        Expr *arg = expr->getArg(i);
        args.push_back(emitCallArgument(fdecl, arg, i));
    }

    if (fdecl->isPrimitive())
        return emitPrimitiveCall(expr, args);
    else if (isLocalCall(expr))
        return emitLocalCall(fdecl, args);
    else if (isDirectCall(expr))
        return emitDirectCall(fdecl, args);
    else
        // We must have an abstract call.
        return CRT.genAbstractCall(Builder, percent, fdecl, args);
}

llvm::Value *CodeGenRoutine::emitCallArgument(SubroutineDecl *srDecl, Expr *arg,
                                              unsigned argPosition)
{
    PM::ParameterMode mode = srDecl->getParamMode(argPosition);

    if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
        return emitVariableReference(arg);
    else
        return emitExpr(arg);
}

llvm::Value *CodeGenRoutine::emitLocalCall(SubroutineDecl *srDecl,
                                           std::vector<llvm::Value *> &args)
{
    // Insert the implicit first parameter, which for a local call is the
    // percent handed to the current subroutine.
    args.insert(args.begin(), percent);

    llvm::Value *func = CG.lookupGlobal(CodeGen::getLinkName(srDecl));
    assert(func && "function lookup failed!");
    return Builder.CreateCall(func, args.begin(), args.end());
}

llvm::Value *CodeGenRoutine::emitDirectCall(SubroutineDecl *srDecl,
                                            std::vector<llvm::Value *> &args)
{
    DomainInstanceDecl *instance;
    Domoid *target;

    // Lookup the domain info structure for the connective.
    instance = cast<DomainInstanceDecl>(srDecl->getDeclRegion());
    target = instance->getDefiningDecl();
    llvm::GlobalValue *capsuleInfo = CG.lookupCapsuleInfo(target);
    assert(capsuleInfo && "Could not resolve info for direct call!");

    // Register the domain of computation with the capsule context.  Using the
    // ID of the instance, index into percent to obtain the appropriate
    // domain_instance.
    unsigned instanceID = CGC.addCapsuleDependency(instance);
    args.insert(args.begin(), CRT.getLocalCapsule(Builder, percent, instanceID));

    llvm::Value *func = CG.lookupGlobal(CodeGen::getLinkName(srDecl));
    assert(func && "function lookup failed!");

    return Builder.CreateCall(func, args.begin(), args.end());
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

llvm::Value *CodeGenRoutine::emitInjExpr(InjExpr *expr)
{
    llvm::Value *op = emitValue(expr->getOperand());

    // If the result type is a scalar type, convert the incomming expression
    // (which must be a pointer type) to a integral value of the needed size.
    if (expr->getType()->isScalarType()) {
        const llvm::Type *loweredTy = CGTypes.lowerType(expr->getType());
        return Builder.CreatePtrToInt(op, loweredTy);
    }

    assert(false && "Cannot codegen inj expression yet!");
    return 0;
}

llvm::Value *CodeGenRoutine::emitPrjExpr(PrjExpr *expr)
{
    Expr *operand = expr->getOperand();
    llvm::Value *val = emitValue(operand);

    // If the operand is a scalar type, its width is no more than that of a
    // pointer.  Extend it to an i8*.
    if (operand->getType()->isScalarType()) {
        const llvm::Type *loweredTy = CGTypes.lowerType(expr->getType());
        return Builder.CreateIntToPtr(val, loweredTy);
    }

    assert(false && "Cannot codegen prj expression yet!");
    return 0;
}
