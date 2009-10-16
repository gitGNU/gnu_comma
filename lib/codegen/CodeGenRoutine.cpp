//===-- codegen/CodeGenRoutine.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/CodeGenCapsule.h"
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
    : CGC(CGC),
      CG(CGC.getCodeGen()),
      CGTypes(CG.getTypeGenerator()),
      CRT(CG.getRuntime()),
      Builder(CG.getLLVMContext()),
      SRDecl(0),
      SRFn(0),
      entryBB(0),
      returnBB(0),
      returnValue(0) { }

void CodeGenRoutine::declareSubroutine(SubroutineDecl *srDecl)
{
    getOrCreateSubroutineDeclaration(srDecl);
}

llvm::Function *
CodeGenRoutine::getOrCreateSubroutineDeclaration(SubroutineDecl *srDecl)
{
    const llvm::FunctionType *srTy = CGTypes.lowerSubroutine(srDecl);
    std::string srName = mangle::getLinkName(CGC.getInstance(), srDecl);
    llvm::Function *fn =
        dyn_cast_or_null<llvm::Function>(CG.lookupGlobal(srName));

    if (!fn) {
        fn = CG.makeFunction(srTy, srName);

        // If this is a function returning a composit type, mark the function
        // as following the struct return calling convention.
        if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(srDecl)) {
            if (fdecl->getReturnType()->isCompositeType()) {
                fn->addAttribute(1, llvm::Attribute::StructRet);

                // FIXME:  This assertion can go one we support variable length
                // return values.
                SubType *subtype = cast<SubType>(fdecl->getReturnType());
                assert(subtype->isConstrained() &&
                       "Variable length return types not supported yet!");
            }
        }

        CG.insertGlobal(srName, fn);
    }
    return fn;
}

void CodeGenRoutine::emitSubroutine(SubroutineDecl *srDecl)
{
    // Remember the declaration we are processing.
    SRDecl = srDecl;

    // Get the llvm function for this routine.
    SRFn = getOrCreateSubroutineDeclaration(srDecl);

    // If the declaration has a pragma import as completion, we are done.
    if (SRDecl->hasPragma(pragma::Import))
        return;

    // Resolve the defining declaration, if needed.
    if (SRDecl->getDefiningDeclaration())
        SRDecl = SRDecl->getDefiningDeclaration();

    injectSubroutineArgs();
    emitSubroutineBody();
    llvm::verifyFunction(*SRFn);
}

/// Generates code for the current subroutines body.
void CodeGenRoutine::emitSubroutineBody()
{
    // Create the return block.
    returnBB = CG.makeBasicBlock("return", SRFn);

    // Create the entry block and set it as the current insertion point for our
    // IRBuilder.
    entryBB = CG.makeBasicBlock("entry", SRFn, returnBB);
    Builder.SetInsertPoint(entryBB);

    // If we are generating a function which is using the struct return calling
    // convention map the return value to the first parameter of this function.
    // Otherwise allocate a stack slot for the return value.
    if (isa<FunctionDecl>(SRDecl)) {
        if (SRFn->hasStructRetAttr())
            returnValue = SRFn->arg_begin();
        else
            returnValue = Builder.CreateAlloca(SRFn->getReturnType());
    }

    // Codegen the function body.  If the resulting insertion context is not
    // properly terminated, create a branch to the return BB.
    llvm::BasicBlock *bodyBB = emitBlockStmt(SRDecl->getBody(), entryBB);
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
    // Extract and save the first implicit argument "%".
    llvm::Function::arg_iterator argI = SRFn->arg_begin();
    llvm::Function::arg_iterator argE = SRFn->arg_begin();
    percent = argI++;

    // For each formal argument, locate the corresponding llvm argument.  This
    // is mostly a one-to-one mapping except when unconstrained arrays are
    // present, in which case there are two arguments (one to the array and one
    // to the bounds).
    //
    // Set the name of each argument to match the corresponding formal.
    SubroutineDecl::const_param_iterator paramI = SRDecl->begin_params();
    SubroutineDecl::const_param_iterator paramE = SRDecl->end_params();
    for ( ; paramI != paramE; ++paramI, ++argI) {
        ParamValueDecl *param = *paramI;
        argI->setName(param->getString());
        declTable[param] = argI;

        if (ArraySubType *arrTy = dyn_cast<ArraySubType>(param->getType())) {
            if (!arrTy->isConstrained())
                boundTable[param] = ++argI;
        }
    }
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

    if (returnValue && !SRFn->hasStructRetAttr()) {
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
        if (ArraySubType *arrTy = dyn_cast<ArraySubType>(objDecl->getType())) {
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

void CodeGenRoutine::emitArrayCopy(llvm::Value *source,
                                   llvm::Value *destination,
                                   ArraySubType *Ty)
{
    // Implement array copies via memcpy.
    llvm::Value *src;
    llvm::Value *dst;
    llvm::Value *len;
    llvm::Constant *align;
    llvm::Function *memcpy;
    const llvm::PointerType *ptrTy;
    const llvm::ArrayType *arrTy;

    src = Builder.CreatePointerCast(source, CG.getInt8PtrTy());
    dst = Builder.CreatePointerCast(destination, CG.getInt8PtrTy());
    ptrTy = cast<llvm::PointerType>(source->getType());
    arrTy = cast<llvm::ArrayType>(ptrTy->getElementType());

    // If the array type is of variable length, use the Comma type to compute
    // the number of elements to copy.
    if (arrTy->getNumElements() == 0)
        len = emitArrayLength(Ty);
    else
        len = llvm::ConstantExpr::getSizeOf(arrTy);

    // Zero extend the length if not an i64.
    if (len->getType() != CG.getInt64Ty())
        len = Builder.CreateZExt(len, CG.getInt64Ty());

    align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    memcpy = CG.getMemcpy64();

    Builder.CreateCall4(memcpy, dst, src, len, align);
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

    if (ArraySubType *arrTy = dyn_cast<ArraySubType>(vTy)) {
        assert(arrTy->isConstrained() && "Unconstrained value declaration!");

        // FIXME: Support multidimensional arrays and general scalar index
        // types.
        IntegerSubType *idxTy = cast<IntegerSubType>(arrTy->getIndexType(0));
        if (!idxTy->isStaticallyConstrained()) {
            const llvm::Type *type;
            type = CGTypes.lowerType(arrTy->getComponentType());

            llvm::BasicBlock *savedBB = Builder.GetInsertBlock();
            Builder.SetInsertPoint(entryBB);
            llvm::Value *length = emitArrayLength(arrTy);
            llvm::Value *stackSlot = Builder.CreateAlloca(type, length);

            // The slot is a pointer to the component type.  Cast it as a
            // pointer to a variable length array.
            type = CG.getPointerType(CGTypes.lowerArraySubType(arrTy));
            stackSlot = Builder.CreatePointerCast(stackSlot, type);

            declTable[decl] = stackSlot;
            Builder.SetInsertPoint(savedBB);
            return stackSlot;
        }
    }

    const llvm::Type *type = CGTypes.lowerType(vDecl->getType());
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

    ArraySubType *arrTy = cast<ArraySubType>(decl->getType());
    const llvm::StructType *boundTy = CGTypes.lowerArrayBounds(arrTy);
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
    if (DeclRefExpr *refExpr = dyn_cast<DeclRefExpr>(expr)) {
        Decl *refDecl = refExpr->getDeclaration();
        llvm::Value *exprValue = lookupDecl(refDecl);

        // If the expression denotes an array type, just return the associated
        // value.  All arrays are manipulated by reference.
        if (expr->getType()->getAsArrayType())
            return exprValue;

        if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            // If the parameter mode is either "out" or "in out" then load the
            // actual value.
            PM::ParameterMode paramMode = pvDecl->getParameterMode();

            if (paramMode == PM::MODE_OUT or paramMode == PM::MODE_IN_OUT)
                return Builder.CreateLoad(exprValue);
            else
                return exprValue;
        }

        // Otherwise, we must have an ObjectDecl.  Just load from the alloca'd
        // stack slot.
        assert(isa<ObjectDecl>(refDecl) && "Unexpected decl kind!");
        return Builder.CreateLoad(exprValue);
    }

    // FIXME:  This is not precise enough, but works for the remaining cases.
    return emitExpr(expr);
}

void CodeGenRoutine::emitPragmaAssert(PragmaAssert *pragma)
{
    llvm::Value *condition = emitValue(pragma->getCondition());
    llvm::GlobalVariable *msgVar = CG.emitInternString(pragma->getMessage());
    llvm::Value *message =
        CG.getPointerCast(msgVar, CG.getPointerType(CG.getInt8Ty()));

    // Create basic blocks for when the assertion fires and another for the
    // continuation.
    llvm::BasicBlock *assertBB = CG.makeBasicBlock("assert-fail", SRFn);
    llvm::BasicBlock *passBB = CG.makeBasicBlock("assert-pass", SRFn);

    // If the condition is true, the assertion does not fire.
    Builder.CreateCondBr(condition, passBB, assertBB);

    // Generate the call to _comma_assert_fail.
    Builder.SetInsertPoint(assertBB);
    CRT.assertFail(Builder, message);

    // Switch to the continuation block.
    Builder.SetInsertPoint(passBB);
}
