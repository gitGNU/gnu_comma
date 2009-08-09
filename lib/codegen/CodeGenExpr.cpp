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
    llvm::Value *argVal = emitExpr(arg);

    // If the argument is a formal of the current subroutine, convert if
    // necessary to satisfy the context mode.
    if (DeclRefExpr *refExpr = dyn_cast<DeclRefExpr>(arg)) {
        Decl *refDecl = refExpr->getDeclaration();
        if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            ParameterMode contextMode = srDecl->getParamMode(argPosition);
            ParameterMode paramMode = pvDecl->getParameterMode();

            // If the argument has a mode being either "out" or "in out", and
            // the context mode is "in", then we must load the parameter value.
            if (contextMode == MODE_IN and
                (paramMode == MODE_OUT or paramMode == MODE_IN_OUT))
                argVal = Builder.CreateLoad(argVal);
        }
    }

    return argVal;
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
    typedef llvm::IntegerType LLVMIntTy;

    llvm::Value *op = emitExpr(expr->getOperand());
    const llvm::Type *targetTy = CGTypes.lowerType(expr->getType());

    assert(isa<llvm::PointerType>(op->getType()) &&
           "Percent expression not a pointer type!");

    // If the target type is an integer type, convert the incomming expression
    // (which must be a pointer type) to a integral value of the needed size.
    if (const LLVMIntTy *intTy = dyn_cast<LLVMIntTy>(targetTy)) {
        assert(intTy->getBitWidth() <= CG.getTargetData().getPointerSizeInBits()
               && "Integral type too large for pointer cast!");
        return Builder.CreatePtrToInt(op, intTy);
    }

    assert(false && "Cannot codegen inj expression yet!");
    return 0;
}

llvm::Value *CodeGenRoutine::emitPrjExpr(PrjExpr *expr)
{
    typedef llvm::IntegerType LLVMIntTy;

    llvm::Value *op = emitExpr(expr->getOperand());
    const llvm::PointerType *percentTy =
        cast<llvm::PointerType>(CGTypes.lowerType(expr->getType()));

    // If the operand is an integer type, its width is no more than that of a
    // pointer.  Extend it to an i8*.
    if (const LLVMIntTy *intTy = dyn_cast<LLVMIntTy>(op->getType())) {
        assert(intTy->getBitWidth() <= CG.getTargetData().getPointerSizeInBits()
               && "Integral type too large for pointer cast!");
        return Builder.CreateIntToPtr(op, percentTy);
    }

    assert(false && "Cannot codegen prj expression yet!");
    return 0;
}
