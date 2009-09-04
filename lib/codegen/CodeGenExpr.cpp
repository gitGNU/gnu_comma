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
using llvm::dyn_cast_or_null;
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

    case Ast::AST_IntegerLiteral:
        val = emitIntegerLiteral(cast<IntegerLiteral>(expr));
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
        return emitAbstractCall(fdecl, args);
}

llvm::Value *CodeGenRoutine::emitCallArgument(SubroutineDecl *srDecl, Expr *arg,
                                              unsigned argPosition)
{
    PM::ParameterMode mode = srDecl->getParamMode(argPosition);

    if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
        return emitVariableReference(arg);
    else
        return emitValue(arg);
}

llvm::Value *CodeGenRoutine::emitLocalCall(SubroutineDecl *srDecl,
                                           std::vector<llvm::Value *> &args)
{
    llvm::Value *func;

    // Insert the implicit first parameter, which for a local call is the
    // percent handed to the current subroutine.
    args.insert(args.begin(), percent);

    if (CGC.generatingParameterizedInstance()) {
        // We are generating a local call from within an instance of a
        // parameterized capsule.  Generate the link name with respect to the
        // current instance and build the call.
        DomainInstanceDecl *instance = CGC.getInstance();
        func = CG.lookupGlobal(CGC.getLinkName(instance, srDecl));
    }
    else
        func = CG.lookupGlobal(CGC.getLinkName(srDecl));

    assert(func && "function lookup failed!");
    return Builder.CreateCall(func, args.begin(), args.end());
}

llvm::Value *CodeGenRoutine::emitDirectCall(SubroutineDecl *srDecl,
                                            std::vector<llvm::Value *> &args)
{
    DomainInstanceDecl *instance
        = cast<DomainInstanceDecl>(srDecl->getDeclRegion());

    // Register the domain of computation with the capsule context.  Using the
    // ID of the instance, index into percent to obtain the appropriate
    // domain_instance.
    unsigned instanceID = CGC.addCapsuleDependency(instance);
    args.insert(args.begin(), CRT.getLocalCapsule(Builder, percent, instanceID));

    AstRewriter rewriter(CG.getAstResource());
    // Always map percent nodes from the current capsule to the instance.
    rewriter[CGC.getCapsule()->getPercent()] = CGC.getInstance()->getType();

    // If the instance is dependent on formal parameters, rewrite using the
    // current parameter map.
    if (instance->isDependent()) {
        const CodeGenCapsule::ParameterMap &paramMap =
            CGC.getParameterMap();
        rewriter.addRewrites(paramMap.begin(), paramMap.end());
    }

    DomainType *targetDomainTy = rewriter.rewrite(instance->getType());
    DomainInstanceDecl *targetInstance = targetDomainTy->getInstanceDecl();
    assert(!targetInstance->isDependent() &&
           "Instance rewriting did not remove all dependencies!");

    // Add the target domain to the code generators worklist.  This will
    // generate forward declarations for the type if they do not already
    // exist.
    CG.extendWorklist(targetInstance);

    // Extend the rewriter with rules mapping the dependent instance type to
    // the rewritten type.  Using this extended rewriter, get the concrete
    // type for the subroutine decl we are calling, and locate it in the
    // target instance (this operation must not fail).
    rewriter.addRewrite(instance->getType(), targetDomainTy);
    SubroutineType *targetTy = rewriter.rewrite(srDecl->getType());
    srDecl = dyn_cast_or_null<SubroutineDecl>(
        targetInstance->findDecl(srDecl->getIdInfo(), targetTy));
    assert(srDecl && "Failed to resolve subroutine!");

    // With our fully-resolved subroutine, get the actual link name and form the
    // call.
    llvm::Value *func = CG.lookupGlobal(CGC.getLinkName(srDecl));
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

    case PO::Plus:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateAdd(args[0], args[1]);

    case PO::Minus:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateSub(args[0], args[1]);

    case PO::LessThan:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateICmpSLT(args[0], args[1]);

    case PO::GreaterThan:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateICmpSGT(args[0], args[1]);

    case PO::LessThanOrEqual:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateICmpSLE(args[0], args[1]);

    case PO::GreaterThanOrEqual:
        assert(args.size() == 2 && "Bad arity for primitive!");
        return Builder.CreateICmpSGE(args[0], args[1]);

    case PO::EnumFunction: {
        EnumLiteral *lit = cast<EnumLiteral>(decl);
        unsigned idx = lit->getIndex();
        const llvm::Type *ty = CGTypes.lowerType(lit->getReturnType());
        return llvm::ConstantInt::get(ty, idx);
    }
    };
}

llvm::Value *CodeGenRoutine::emitAbstractCall(SubroutineDecl *srDecl,
                                              std::vector<llvm::Value *> &args)
{
    // Resolve the region for this declaration, which is asserted to be an
    // AbstractDomainDecl.
    AbstractDomainDecl *abstract =
        cast<AbstractDomainDecl>(srDecl->getDeclRegion());
    DomainType *abstractTy = abstract->getType();

    // Resolve the abstract domain to a concrete type using the parameter map
    // provided by the capsule context.
    DomainInstanceDecl *instance = CGC.rewriteAbstractDecl(abstract);
    assert(instance && "Failed to resolve abstract domain!");

    // Add this instance to the code generators worklist, thereby ensuring
    // forward declarations are generated and that the implementation will be
    // codegened.
    CG.extendWorklist(instance);

    // The instance must provide a subroutine declaration with a type matching
    // that of the original, with the only exception being that occurrences of
    // abstractTy are mapped to the type of the resolved instance.  Use an
    // AstRewriter to obtain the required target type.
    AstRewriter rewriter(CG.getAstResource());
    rewriter.addRewrite(abstractTy, instance->getType());
    SubroutineType *targetTy = rewriter.rewrite(srDecl->getType());

    // Lookup the target declaration in the instance.  This must not fail.
    SubroutineDecl *resolvedRoutine = dyn_cast_or_null<SubroutineDecl>(
        instance->findDecl(srDecl->getIdInfo(), targetTy));
    assert(resolvedRoutine && "Failed to resolve abstract subroutine!");

    // Index into percent to obtain the actual domain instance serving as a
    // parameter.
    FunctorDecl *functor = cast<FunctorDecl>(CGC.getCapsule());
    unsigned index = functor->getFormalIndex(abstract);
    llvm::Value *DOC = CRT.getCapsuleParameter(Builder, percent, index);

    // Insert the DOC as the implicit first parameter for the call, obtain the
    // actual llvm function representing the instances subroutine, and emit the
    // call.
    args.insert(args.begin(), DOC);
    llvm::Value *func = CG.lookupGlobal(CGC.getLinkName(resolvedRoutine));
    assert(func && "function lookup failed!");
    return Builder.CreateCall(func, args.begin(), args.end());
}

llvm::Value *CodeGenRoutine::emitInjExpr(InjExpr *expr)
{
    return emitValue(expr->getOperand());
}

llvm::Value *CodeGenRoutine::emitPrjExpr(PrjExpr *expr)
{
    return emitValue(expr->getOperand());
}

llvm::Value *CodeGenRoutine::emitIntegerLiteral(IntegerLiteral *expr)
{
    assert(expr->hasType() && "Unresolved literal type!");

    const llvm::IntegerType *ty =
        llvm::cast<llvm::IntegerType>(CGTypes.lowerType(expr->getType()));
    llvm::APInt val(expr->getValue());

    // All comma integer types are represented as signed.  Sign extend the value
    // if needed.
    unsigned valWidth = val.getBitWidth();
    unsigned tyWidth = ty->getBitWidth();
    assert(valWidth <= tyWidth && "Value/Type width mismatch!");

    if (valWidth < tyWidth)
        val.sext(tyWidth);

    return llvm::ConstantInt::get(CG.getLLVMContext(), val);
}
