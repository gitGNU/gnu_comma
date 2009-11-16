//===-- codegen/CodeGenRoutine.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenCapsule.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "SRInfo.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Stmt.h"
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
            if (!arrTy->isConstrained()) {
                ++argI;
                std::string boundName(param->getString());
                boundName += ".bounds";
                argI->setName(boundName);
                boundTable[param] = argI;
            }
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

bool CodeGenRoutine::isDirectCall(const SubroutineCall *call)
{
    if (call->isAmbiguous())
        return false;

    const SubroutineDecl *decl = call->getConnective();
    const DeclRegion *region = decl->getDeclRegion();
    return isa<DomainInstanceDecl>(region);
}

bool CodeGenRoutine::isLocalCall(const SubroutineCall *call)
{
    if (call->isAmbiguous())
        return false;

    // FIXME: This is a hack.  We rely here on the esoteric property that a
    // local decl is declared in an "add" context.  Rather, check that the decl
    // originates from the curent domain, or explicity tag decls as local in the
    // AST.
    const SubroutineDecl *decl = call->getConnective();
    const DeclRegion *region = decl->getDeclRegion();
    return isa<AddDecl>(region);
}

bool CodeGenRoutine::isForeignCall(const SubroutineCall *call)
{
    if (call->isAmbiguous())
        return false;

    const SubroutineDecl *srDecl = call->getConnective();
    return srDecl->hasPragma(pragma::Import);
}

void CodeGenRoutine::emitObjectDecl(ObjectDecl *objDecl)
{
    Type *objTy = objDecl->getType();

    if (ArrayType *arrTy = dyn_cast<ArrayType>(objTy)) {
        BoundsEmitter emitter(*this);
        llvm::Value *bounds = createBounds(objDecl);
        llvm::Value *slot = 0;

        if (arrTy->isStaticallyConstrained())
            slot = createStackSlot(objDecl);

        if (objDecl->hasInitializer()) {
            Expr *init = objDecl->getInitializer();
            if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(init)) {
                // Only staticly constrained array types are delt with here.
                assert(arrTy->isStaticallyConstrained());

                // Perform the function call and add the destination to the
                // argument set.
                emitCompositeCall(call, slot);

                // Synthesize bounds for this declaration.
                emitter.synthStaticArrayBounds(Builder, arrTy, bounds);
            }
            else {
                std::pair<llvm::Value*, llvm::Value*> result;
                result = emitArrayExpr(init, slot, true);
                if (!slot)
                    associateStackSlot(objDecl, result.first);
                Builder.CreateStore(result.second, bounds);
            }
        }
        else
            emitter.synthStaticArrayBounds(Builder, arrTy, bounds);
        return;
    }

    // Otherwise, this is a simple non-composite type.  Allocate a stack slot
    // and evaluate the initializer if present.
    llvm::Value *slot = createStackSlot(objDecl);
    if (objDecl->hasInitializer()) {
        llvm::Value *value = emitValue(objDecl->getInitializer());
        Builder.CreateStore(value, slot);
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

llvm::Value *CodeGenRoutine::createStackSlot(ObjectDecl *decl)
{
    assert(lookupDecl(decl) == 0 &&
           "Cannot create stack slot for preexisting decl!");

    const llvm::Type *type = CGT.lowerType(decl->getType());
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    llvm::Value *stackSlot = Builder.CreateAlloca(type);
    declTable[decl] = stackSlot;
    Builder.SetInsertPoint(savedBB);
    return stackSlot;
}

llvm::Value *CodeGenRoutine::createTemp(const llvm::Type *type)
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
    associateBounds(decl, bounds);
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
        ValueDecl *refDecl = refExpr->getDeclaration();
        llvm::Value *addr = 0;

        if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            // Ensure that the parameter has a mode consistent with reference
            // emission.
            PM::ParameterMode paramMode = pvDecl->getParameterMode();
            assert((paramMode == PM::MODE_OUT || paramMode == PM::MODE_IN_OUT)
                   && "Cannot take reference to a parameter with mode IN!");
            addr = lookupDecl(pvDecl);
        }
        else {
            // Otherwise, we must have a local object declaration.  Simply
            // return the associated stack slot.
            ObjectDecl *objDecl = cast<ObjectDecl>(refDecl);
            addr = getStackSlot(objDecl);
        }
        return addr;
    }
    else if (IndexedArrayExpr *idxExpr = dyn_cast<IndexedArrayExpr>(expr))
        return emitIndexedArrayRef(idxExpr);

    assert(false && "Cannot codegen reference for expression!");
    return 0;
}

llvm::Value *CodeGenRoutine::emitValue(Expr *expr)
{
    switch (expr->getKind()) {

    default:
        if (AttribExpr *attrib = dyn_cast<AttribExpr>(expr))
            return emitAttribExpr(attrib);
        else
            assert(false && "Cannot codegen expression!");
        break;

    case Ast::AST_DeclRefExpr:
        return emitDeclRefExpr(cast<DeclRefExpr>(expr));

    case Ast::AST_FunctionCallExpr:
        return emitSimpleCall(cast<FunctionCallExpr>(expr));

    case Ast::AST_InjExpr:
        return emitInjExpr(cast<InjExpr>(expr));

    case Ast::AST_PrjExpr:
        return emitPrjExpr(cast<PrjExpr>(expr));

    case Ast::AST_IntegerLiteral:
        return emitIntegerLiteral(cast<IntegerLiteral>(expr));

    case Ast::AST_IndexedArrayExpr:
        return emitIndexedArrayValue(cast<IndexedArrayExpr>(expr));

    case Ast::AST_ConversionExpr:
        return emitConversionValue(cast<ConversionExpr>(expr));
    }
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
