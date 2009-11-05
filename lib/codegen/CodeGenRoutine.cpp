//===-- codegen/CodeGenRoutine.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenCapsule.h"
#include "SRInfo.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"
#include "comma/codegen/Mangle.h"

#include "llvm/Analysis/Verifier.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

CodeGenRoutine::CodeGenRoutine(CodeGenCapsule &CGC)
    : CG(CGC.getCodeGen()),
      CGC(CGC),
      CGT(CGC.getTypeGenerator()),
      CRT(CG.getRuntime()),
      Builder(CG.getLLVMContext()),
      entryBB(0),
      returnBB(0),
      returnValue(0) { }

void CodeGenRoutine::emitSubroutine(SubroutineDecl *srDecl)
{
    // Remember the declaration we are processing.
    srInfo = CG.getSRInfo(CGC.getInstance(), srDecl);

    // If this declaration is imported (pragma import as completion), we are
    // done.
    if (srInfo->isImported())
        return;

    // Resolve the completion for this subroutine, if needed.
    srCompletion = srDecl->getDefiningDeclaration();
    if (!srCompletion)
        srCompletion = srDecl;

    injectSubroutineArgs();
    emitSubroutineBody();
    llvm::verifyFunction(*srInfo->getLLVMFunction());
}

/// Generates code for the current subroutines body.
void CodeGenRoutine::emitSubroutineBody()
{
    // Create the return block.
    returnBB = makeBasicBlock("return");

    // Create the entry block and set it as the current insertion point for our
    // IRBuilder.
    entryBB = makeBasicBlock("entry", returnBB);
    Builder.SetInsertPoint(entryBB);

    // If we are generating a function which is using the struct return calling
    // convention map the return value to the first parameter of this function.
    // Otherwise allocate a stack slot for the return value.
    if (srInfo->isaFunction()) {
        llvm::Function *SRFn = getLLVMFunction();
        if (srInfo->hasSRet())
            returnValue = SRFn->arg_begin();
        else
            returnValue = Builder.CreateAlloca(SRFn->getReturnType());
    }

    // Codegen the function body.  If the resulting insertion context is not
    // properly terminated, create a branch to the return BB.
    BlockStmt *body = srCompletion->getBody();
    llvm::BasicBlock *bodyBB = emitBlockStmt(body, entryBB);
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(returnBB);

    emitPrologue(bodyBB);
    emitEpilogue();
}

/// Given the current SubroutineDecl and llvm::Function, initialize
/// CodeGenRoutine::percent with the llvm value corresponding to the first
/// (implicit) argument.  Also, name the llvm arguments after the source
/// formals, and populate the lookup tables such that a search for a parameter
/// decl yields the corresponding llvm value.
void CodeGenRoutine::injectSubroutineArgs()
{
    llvm::Function *SRFn = getLLVMFunction();
    llvm::Function::arg_iterator argI = SRFn->arg_begin();

    // If this function uses the SRet convention, name the return argument.
    if (srInfo->hasSRet()) {
        argI->setName("return.arg");
        ++argI;
    }

    // The next argument is the domain instance structure.  Name the arg
    // "percent".
    argI->setName("percent");
    percent = argI++;

    // For each formal argument, locate the corresponding llvm argument.  This
    // is mostly a one-to-one mapping except when unconstrained arrays are
    // present, in which case there are two arguments (one to the array and one
    // to the bounds).
    //
    // Set the name of each argument to match the corresponding formal.
    SubroutineDecl::const_param_iterator paramI = srCompletion->begin_params();
    SubroutineDecl::const_param_iterator paramE = srCompletion->end_params();
    for ( ; paramI != paramE; ++paramI, ++argI) {
        ParamValueDecl *param = *paramI;
        argI->setName(param->getString());
        declTable[param] = argI;

        if (ArrayType *arrTy = dyn_cast<ArrayType>(param->getType())) {
            if (!arrTy->isConstrained())
                boundTable[param] = ++argI;
        }
    }
}

llvm::Function *CodeGenRoutine::getLLVMFunction() const
{
    return srInfo->getLLVMFunction();
}

void CodeGenRoutine::emitPrologue(llvm::BasicBlock *bodyBB)
{
    // FIXME:  In the future, we will have accumulated much state about what the
    // prologue should contain.  For now, just branch to the entry block for the
    // body.
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(savedBB);
}

void CodeGenRoutine::emitEpilogue()
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(returnBB);

    if (returnValue && !srInfo->hasSRet()) {
        llvm::Value *V = Builder.CreateLoad(returnValue);
        Builder.CreateRet(V);
    }
    else
        Builder.CreateRetVoid();
    Builder.SetInsertPoint(savedBB);
}

llvm::Value *CodeGenRoutine::lookupDecl(Decl *decl)
{
    DeclMap::iterator iter = declTable.find(decl);

    if (iter != declTable.end())
        return iter->second;
    return 0;
}

bool CodeGenRoutine::isDirectCall(const FunctionCallExpr *expr)
{
    const FunctionDecl *decl = expr->getConnective(0);
    if (decl) {
        const DeclRegion *region = decl->getDeclRegion();
        return isa<DomainInstanceDecl>(region);
    }
    return false;
}

bool CodeGenRoutine::isDirectCall(const ProcedureCallStmt *stmt)
{
    const ProcedureDecl *pdecl = stmt->getConnective();
    const DeclRegion *region = pdecl->getDeclRegion();
    return isa<DomainInstanceDecl>(region);
}

bool CodeGenRoutine::isLocalCall(const FunctionCallExpr *expr)
{
    const FunctionDecl *decl = expr->getConnective(0);
    if (decl) {
        // FIXME: This is a hack.  We rely here on the esoteric property that a
        // local decl is declared in an "add" context.  Rather, check that the
        // decl originates from the curent domain, or explicity tag decls as
        // local in the AST.
        const DeclRegion *region = decl->getDeclRegion();
        return isa<AddDecl>(region);
    }
    return false;
}

bool CodeGenRoutine::isLocalCall(const ProcedureCallStmt *stmt)
{
    // FIXME: This is a hack.  We rely here on the esoteric property that a
    // local decl is declared in an "add" context.  Rather, check that the decl
    // originates from the curent domain, or explicity tag decls as local in the
    // AST.
    const ProcedureDecl *pdecl = stmt->getConnective();
    const DeclRegion *region = pdecl->getDeclRegion();
    return isa<AddDecl>(region);
}

void CodeGenRoutine::emitObjectDecl(ObjectDecl *objDecl)
{
    if (objDecl->hasInitializer()) {
        llvm::Value *init = emitValue(objDecl->getInitializer());
        if (ArrayType *arrTy = dyn_cast<ArrayType>(objDecl->getType())) {
            // FIXME: It would be very nice to optimize array assignment by
            // reusing any temporaries generated on the RHS.  However, LLVM's
            // optimizers can do the thinking for us here most of the time.
            llvm::Value *slot = createStackSlot(objDecl);
            emitArrayCopy(init, slot, arrTy);
        }
        else
            Builder.CreateStore(init, createStackSlot(objDecl));
    }
    else {
        // FIXME:  We should be giving all objects default values here.
        createStackSlot(objDecl);
    }
}

llvm::Value *CodeGenRoutine::emitScalarLoad(llvm::Value *ptr)
{
    return Builder.CreateLoad(ptr);
}

llvm::Value *CodeGenRoutine::getStackSlot(Decl *decl)
{
    // FIXME: We should be more precise.  Subroutine parameters, for example,
    // are accessible via lookupDecl.  There is no way (ATM) to distinguish a
    // parameter from a alloca'd local.  The lookup methods should provide finer
    // grained resolution.
    llvm::Value *stackSlot = lookupDecl(decl);
    assert(stackSlot && "Lookup failed!");
    return stackSlot;
}

llvm::Value *CodeGenRoutine::createStackSlot(Decl *decl)
{
    assert(lookupDecl(decl) == 0 &&
           "Cannot create stack slot for preexisting decl!");

    // The only kind of declaration we emit stack slots for are value
    // declarations, hense the cast.
    ValueDecl *vDecl = cast<ValueDecl>(decl);
    Type *vTy = vDecl->getType();

    if (ArrayType *arrTy = dyn_cast<ArrayType>(vTy)) {
        assert(arrTy->isConstrained() && "Unconstrained value declaration!");

        // FIXME: Support multidimensional arrays and general discrete index
        // types.
        IntegerType *idxTy = cast<IntegerType>(arrTy->getIndexType(0));
        if (!idxTy->isStaticallyConstrained()) {
            const llvm::Type *type;
            type = CGT.lowerType(arrTy->getComponentType());

            llvm::BasicBlock *savedBB = Builder.GetInsertBlock();
            Builder.SetInsertPoint(entryBB);
            llvm::Value *length = emitArrayLength(arrTy);
            llvm::Value *stackSlot = Builder.CreateAlloca(type, length);

            // The slot is a pointer to the component type.  Cast it as a
            // pointer to a variable length array.
            type = CG.getPointerType(CGT.lowerArrayType(arrTy));
            stackSlot = Builder.CreatePointerCast(stackSlot, type);

            declTable[decl] = stackSlot;
            Builder.SetInsertPoint(savedBB);
            return stackSlot;
        }
    }

    const llvm::Type *type = CGT.lowerType(vDecl->getType());
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    llvm::Value *stackSlot = Builder.CreateAlloca(type);
    declTable[decl] = stackSlot;
    Builder.SetInsertPoint(savedBB);
    return stackSlot;
}

llvm::Value *CodeGenRoutine::getOrCreateStackSlot(Decl *decl)
{
    if (llvm::Value *stackSlot = lookupDecl(decl))
        return stackSlot;
    else
        return createStackSlot(decl);
}

llvm::Value *CodeGenRoutine::createTemporary(const llvm::Type *type)
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    llvm::Value *stackSlot = Builder.CreateAlloca(type);
    Builder.SetInsertPoint(savedBB);
    return stackSlot;
}

void CodeGenRoutine::associateStackSlot(Decl *decl, llvm::Value *value)
{
    assert(!lookupDecl(decl) && "Decl already associated with a stack slot!");
    declTable[decl] = value;
}

llvm::Value *CodeGenRoutine::lookupBounds(ValueDecl *decl)
{
    DeclMap::iterator iter = boundTable.find(decl);

    if (iter != boundTable.end())
        return iter->second;
    return 0;
}

llvm::Value *CodeGenRoutine::createBounds(ValueDecl *decl)
{
    assert(!lookupBounds(decl) && "Decl already associated with bounds!");

    ArrayType *arrTy = cast<ArrayType>(decl->getType());
    const llvm::StructType *boundTy = CGT.lowerArrayBounds(arrTy);
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    llvm::Value *bounds = Builder.CreateAlloca(boundTy);
    Builder.SetInsertPoint(savedBB);
    return bounds;
}

void CodeGenRoutine::associateBounds(ValueDecl *decl, llvm::Value *value)
{
    assert(!lookupBounds(decl) && "Decl already associated with bounds!");
    boundTable[decl] = value;
}

llvm::Value *CodeGenRoutine::emitVariableReference(Expr *expr)
{
    if (DeclRefExpr *refExpr = dyn_cast<DeclRefExpr>(expr)) {
        Decl *refDecl = refExpr->getDeclaration();

        if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            PM::ParameterMode paramMode = pvDecl->getParameterMode();
            // Enusure that the parameter has a mode consistent with reference
            // emission.
            assert((paramMode == PM::MODE_OUT || paramMode == PM::MODE_IN_OUT)
                   && "Cannot take reference to a parameter with mode IN!");
            return lookupDecl(pvDecl);
        }
        if (ObjectDecl *objDecl = dyn_cast<ObjectDecl>(refDecl)) {
            // Local object declarations are always generated with repect to a
            // stack slot.
            return getStackSlot(objDecl);
        }
        assert(false && "Cannot codegen reference for expression!");
    }
    else if (IndexedArrayExpr *idxExpr = dyn_cast<IndexedArrayExpr>(expr))
        return emitIndexedArrayRef(idxExpr);
    else {
        assert(false && "Cannot codegen reference for expression!");
    }
}

llvm::Value *CodeGenRoutine::emitValue(Expr *expr)
{
    llvm::Value *result;

    switch (expr->getKind()) {

    default:
        if (AttribExpr *attrib = dyn_cast<AttribExpr>(expr))
            result = emitAttribExpr(attrib);
        else
            assert(false && "Cannot codegen expression!");
        break;

    case Ast::AST_DeclRefExpr: {
        DeclRefExpr *refExpr = cast<DeclRefExpr>(expr);
        Decl *refDecl = refExpr->getDeclaration();
        llvm::Value *exprValue = lookupDecl(refDecl);

        if (expr->getType()->getAsArrayType()) {
            // If the expression denotes an array type, just return the
            // associated value.  All arrays are manipulated by reference.
            result = exprValue;
        }
        else if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            // If the parameter mode is either "out" or "in out" then load the
            // actual value.
            PM::ParameterMode paramMode = pvDecl->getParameterMode();

            if (paramMode == PM::MODE_OUT or paramMode == PM::MODE_IN_OUT)
                result = Builder.CreateLoad(exprValue);
            else
                result = exprValue;
        }
        else {
            // Otherwise, we must have an ObjectDecl.  Just load from the
            // alloca'd stack slot.
            assert(isa<ObjectDecl>(refDecl) && "Unexpected decl kind!");
            result = Builder.CreateLoad(exprValue);
        }
        break;
    }

    case Ast::AST_FunctionCallExpr:
        result = emitFunctionCall(cast<FunctionCallExpr>(expr));
        break;

    case Ast::AST_InjExpr:
        result = emitInjExpr(cast<InjExpr>(expr));
        break;

    case Ast::AST_PrjExpr:
        result = emitPrjExpr(cast<PrjExpr>(expr));
        break;

    case Ast::AST_IntegerLiteral:
        result = emitIntegerLiteral(cast<IntegerLiteral>(expr));
        break;

    case Ast::AST_StringLiteral:
        result = emitStringLiteral(cast<StringLiteral>(expr));
        break;

    case Ast::AST_IndexedArrayExpr:
        result = emitIndexedArrayValue(cast<IndexedArrayExpr>(expr));
        break;

    case Ast::AST_ConversionExpr:
        result = emitConversionValue(cast<ConversionExpr>(expr));
        break;
    }

    return result;
}

void CodeGenRoutine::emitPragmaAssert(PragmaAssert *pragma)
{
    llvm::Value *condition = emitValue(pragma->getCondition());
    llvm::GlobalVariable *msgVar = CG.emitInternString(pragma->getMessage());
    llvm::Value *message =
        CG.getPointerCast(msgVar, CG.getPointerType(CG.getInt8Ty()));

    // Create basic blocks for when the assertion fires and another for the
    // continuation.
    llvm::BasicBlock *assertBB = makeBasicBlock("assert-fail");
    llvm::BasicBlock *passBB = makeBasicBlock("assert-pass");

    // If the condition is true, the assertion does not fire.
    Builder.CreateCondBr(condition, passBB, assertBB);

    // Generate the call to _comma_assert_fail.
    Builder.SetInsertPoint(assertBB);
    CRT.assertFail(Builder, message);

    // Switch to the continuation block.
    Builder.SetInsertPoint(passBB);
}

//===----------------------------------------------------------------------===//
// LLVM IR generation helpers.

llvm::BasicBlock *
CodeGenRoutine::makeBasicBlock(const std::string &name,
                               llvm::BasicBlock *insertBefore) const
{
    return CG.makeBasicBlock(name, getLLVMFunction(), insertBefore);
}
