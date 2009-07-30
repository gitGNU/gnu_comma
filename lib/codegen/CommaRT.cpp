//===-- codegen/CommaRT.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/Module.h"
#include "llvm/Value.h"
#include "llvm/Support/IRBuilder.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CommaRT::DomainInfoTy::DomainInfoTy(const llvm::TargetData &TD,
                                    const llvm::PointerType *CtorTy,
                                    const llvm::PointerType *ITableTy,
                                    const llvm::PointerType *ExportFnTy)
    : TD(TD),
      theType(0)
{
    std::vector<const llvm::Type*> members;
    const llvm::Type* IntPtrTy = TD.getIntPtrType();

    members.push_back(llvm::PointerType::getUnqual(llvm::Type::Int8Ty)); // name
    members.push_back(llvm::Type::Int32Ty);                    // arity
    members.push_back(CtorTy);                                 // ctor
    members.push_back(ITableTy);                               // instance_table
    members.push_back(llvm::Type::Int32Ty);                    // num_signatures
    members.push_back(llvm::PointerType::getUnqual(IntPtrTy)); // sig_offsets
    members.push_back(llvm::PointerType::getUnqual(ExportFnTy)); // exports

    theType = llvm::StructType::get(members);
}

const std::string CommaRT::DomainInfoTy::theTypeName("_comma_domain_info_t");

const llvm::PointerType *CommaRT::DomainInfoTy::getPointerTypeTo() const
{
    return llvm::PointerType::getUnqual(theType);
}

const llvm::Type *CommaRT::DomainInfoTy::getFieldType(FieldID ID) const
{
    switch (ID) {
    default:
        assert(false && "Bad field ID!");
        return 0;

    case Name:
        return theType->getElementType(0);

    case Arity:
        return theType->getElementType(1);

    case Ctor:
        return theType->getElementType(2);

    case ITable:
        return theType->getElementType(3);

    case NumSigs:
        return theType->getElementType(4);

    case SigOffsets:
        return theType->getElementType(5);

    case Exvec:
        return theType->getElementType(6);
    }
}


CommaRT::CommaRT(CodeGen &CG)
    : CG(CG),
      ITableName("_comma_itable_t"),
      ExportFnName("_comma_export_fn_t"),
      DomainViewName("_comma_domain_view_t"),
      DomainInstanceName("_comma_domain_instance_t"),
      DomainCtorName("_comma_domain_ctor_t"),

      ExportFnPtrTy(getExportFnPtrTy()),
      ITablePtrTy(getITablePtrTy()),

      DomainViewTy(llvm::OpaqueType::get()),
      DomainInstanceTy(llvm::OpaqueType::get()),
      DomainViewPtrTy(llvm::PointerType::getUnqual(DomainViewTy)),
      DomainInstancePtrTy(llvm::PointerType::getUnqual(DomainInstanceTy)),
      DomainCtorPtrTy(getDomainCtorPtrTy()),

      DIType(CG.getTargetData(), DomainCtorPtrTy, ITablePtrTy, ExportFnPtrTy),

      GetDomainName("_comma_get_domain")
{
    generateRuntimeTypes();
    generateRuntimeFunctions();
}

const llvm::Type *CommaRT::getType(TypeId id) const
{
    switch (id) {
    default:
        assert(false && "Invalid type id!");
        return 0;
    case CRT_ExportFn:
        return ExportFnPtrTy;
    case CRT_ITable:
        return ITablePtrTy;
    case CRT_DomainInfo:
        return DIType.getPointerTypeTo();
    case CRT_DomainView:
        return DomainViewPtrTy;
    case CRT_DomainInstance:
        return DomainInstancePtrTy;
    case CRT_DomainCtor:
        return DomainCtorPtrTy;
    }
}

const std::string &CommaRT::getTypeName(TypeId id) const
{
    switch (id) {
    default:
        assert(false && "Invalid type id!");
        return InvalidName;
    case CRT_ExportFn:
        return ExportFnName;
    case CRT_ITable:
        return ITableName;
    case CRT_DomainInfo:
        return DIType.getTypeName();
    case CRT_DomainView:
        return DomainViewName;
    case CRT_DomainInstance:
        return DomainInstanceName;
    case CRT_DomainCtor:
        return DomainCtorName;
    }
}

void CommaRT::generateRuntimeTypes()
{
    // Generate type types.
    defineDomainViewTy();
    defineDomainInstanceTy();

    // Define the types within the Module.
    llvm::Module *M = CG.getModule();
    M->addTypeName(getTypeName(CRT_ExportFn),       getType(CRT_ExportFn));
    M->addTypeName(getTypeName(CRT_ITable),         getType(CRT_ITable));
    M->addTypeName(getTypeName(CRT_DomainInfo),     getType(CRT_DomainInfo));
    M->addTypeName(getTypeName(CRT_DomainView),     getType(CRT_DomainView));
    M->addTypeName(getTypeName(CRT_DomainInstance), getType(CRT_DomainInstance));
    M->addTypeName(getTypeName(CRT_DomainCtor),     getType(CRT_DomainCtor));
}

llvm::PointerType *CommaRT::getDomainCtorPtrTy()
{
    std::vector<const llvm::Type*> args;

    args.push_back(DomainInstancePtrTy);

    const llvm::Type *ctorTy = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);
    return llvm::PointerType::getUnqual(ctorTy);
}

llvm::PointerType *CommaRT::getExportFnPtrTy()
{
    std::vector<const llvm::Type*> args;
    llvm::Type *ftype;

    ftype = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);
    return llvm::PointerType::getUnqual(ftype);
}

llvm::PointerType *CommaRT::getITablePtrTy()
{
    return llvm::PointerType::getUnqual(llvm::OpaqueType::get());
}

void CommaRT::defineDomainViewTy()
{
    std::vector<const llvm::Type*> members;
    const llvm::Type* IntPtrTy = CG.getTargetData().getIntPtrType();
    members.push_back(DomainInstancePtrTy); // instance
    members.push_back(IntPtrTy);            // index
    members.push_back(ExportFnPtrTy);       // exports
    llvm::StructType *ViewTy = llvm::StructType::get(members);
    cast<llvm::OpaqueType>(DomainViewTy.get())->refineAbstractTypeTo(ViewTy);
}

void CommaRT::defineDomainInstanceTy()
{
    std::vector<const llvm::Type*> members;
    members.push_back(DIType.getPointerTypeTo());                     // info
    members.push_back(DomainInstancePtrTy);                           // next
    members.push_back(llvm::PointerType::getUnqual(DomainViewPtrTy)); // params
    members.push_back(llvm::PointerType::getUnqual(DomainViewPtrTy)); // views
    members.push_back(llvm::PointerType::getUnqual(DomainInstancePtrTy)); // required
    llvm::StructType *InstanceTy = llvm::StructType::get(members);
    cast<llvm::OpaqueType>(DomainInstanceTy.get())->refineAbstractTypeTo(InstanceTy);
}

void CommaRT::generateRuntimeFunctions()
{
    defineGetDomain();
}

// Builds a declaration in LLVM IR for the get_domain runtime function.
void CommaRT::defineGetDomain()
{
    const llvm::Type *retTy = getType(CRT_DomainInstance);
    std::vector<const llvm::Type *> args;

    args.push_back(getType(CRT_DomainInfo));

    // get_domain takes a pointer to a domain_instance_t as first argument, and
    // then a variable number of domain_view_t args corresponding to the
    // parameters.
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, true);

    getDomainFn = llvm::Function::Create(fnTy,
                                         llvm::GlobalValue::ExternalLinkage,
                                         GetDomainName, CG.getModule());
}

llvm::GlobalVariable *CommaRT::registerCapsule(CodeGenCapsule &CGC)
{
    // We must emit a domain_instance_t object into the current module
    // representing the given capsule definition.
    std::vector<llvm::Constant *> elts;
    Domoid *theCapsule = CGC.getCapsule();

    // Types for each field of a domain_info structure.
    const llvm::Type *DINameTy = DIType.getFieldType(DomainInfoTy::Name);
    const llvm::Type *DIArityTy = DIType.getFieldType(DomainInfoTy::Arity);
    const llvm::Type *DIITabTy = DIType.getFieldType(DomainInfoTy::ITable);
    const llvm::Type *DIExportsTy = DIType.getFieldType(DomainInfoTy::Exvec);

    // First, allocate a constant string to hold the name of the capsule.
    llvm::Constant *capsuleName = CG.emitStringLiteral(theCapsule->getString());
    elts.push_back(llvm::ConstantExpr::getPointerCast(capsuleName, DINameTy));

    // Fill in this capsules arity.
    if (FunctorDecl *functor = dyn_cast<FunctorDecl>(theCapsule))
        elts.push_back(llvm::ConstantInt::get(DIArityTy, functor->getArity()));
    else
        elts.push_back(llvm::ConstantInt::get(DIArityTy, 0));

    // Generate the constructor function.
    elts.push_back(genCapsuleCtor(CGC));

    // Initially the instance table is always 0.  A table is allocated at
    // runtime when needed.
    elts.push_back(llvm::ConstantPointerNull::get(
                       cast<llvm::PointerType>(DIITabTy)));

    // Populate the signature count.
    elts.push_back(genSignatureCount(CGC));

    // Generate the signature offset array.
    elts.push_back(genSignatureOffsets(CGC));

    // FIXME: Generate the export array.
    elts.push_back(llvm::ConstantPointerNull::get(cast<llvm::PointerType>(DIExportsTy)));

    // Create the global for this info object.
    std::string infoLinkName = CGC.getLinkName() + "__0domain_info";
    llvm::Constant *theInfo = llvm::ConstantStruct::get(DIType.getType(), elts);
    return new llvm::GlobalVariable(theInfo->getType(), false,
                                    llvm::GlobalValue::ExternalLinkage,
                                    theInfo, infoLinkName, CG.getModule());
}

llvm::Value *CommaRT::getDomain(llvm::IRBuilder<> &builder,
                                llvm::GlobalValue *capsuleInfo) const
{
    return builder.CreateCall(getDomainFn, capsuleInfo);
}


llvm::Value *CommaRT::getLocalCapsule(llvm::IRBuilder<> &builder,
                                      llvm::Value *percent, unsigned ID) const
{
    assert(percent->getType() == DomainInstancePtrTy &&
           "Bad type for percent value!");

    // Index into percent giving a pointer to the required capsule array member.
    llvm::Value *elt;
    elt = builder.CreateStructGEP(percent, 4);

    // Load the required capsule array.
    elt = builder.CreateLoad(elt);

    // Index into the required capsule array by the given ID.
    elt = builder.CreateGEP(elt,
                            llvm::ConstantInt::get(llvm::Type::Int32Ty, ID));

    // And finally load the indexed pointer, yielding the local capsule.
    return builder.CreateLoad(elt);
}

llvm::Constant *CommaRT::genCapsuleCtor(CodeGenCapsule &CGC)
{
    // If the capsule in question does not have any dependencies, do not build a
    // function -- just return 0.  The runtime will not call thru null
    // constructors.
    if (CGC.dependencyCount() == 0) {
        return llvm::ConstantPointerNull::get(DomainCtorPtrTy);
    }

    llvm::Module *M = CG.getModule();
    std::string ctorName = CGC.getLinkName() + "__0ctor";
    const llvm::FunctionType *ctorTy;
    llvm::Function *ctor;

    ctorTy = cast<llvm::FunctionType>(DomainCtorPtrTy->getElementType());
    ctor = llvm::Function::Create(ctorTy,
                                  llvm::Function::ExternalLinkage,
                                  ctorName, M);
    ctor->setDoesNotThrow();

    CG.insertGlobal(ctorName, ctor);

    // Create a basic block to hold a sequence of _comma_get_domain calls which
    // populates the "required capsules" vector with the needed instances.  Once
    // we have generated this block, we will generate code to allocate an
    // appropriately sized array.
    llvm::BasicBlock *constructBB = llvm::BasicBlock::Create("construct", ctor);
    llvm::IRBuilder<> builder;
    builder.SetInsertPoint(constructBB);

    // The first (and only) argument of the constructor is a domain_instance_t.
    llvm::Value *instance = &(ctor->getArgumentList().front());

    // Extract a pointer to the "required capsules" array.
    llvm::Value *capsules = builder.CreateStructGEP(instance, 4);
    capsules = builder.CreateLoad(capsules);

    // Iterate over the set of capsule dependencies and emit calls to
    // _comma_get_domain for each, keeping track of the number of dependents.
    unsigned numDependents = 0;
    for (unsigned i = 0; i < CGC.dependencyCount(); ++i) {
        DomainInstanceDecl *dependent = CGC.getCapsuleDependency(i);
        numDependents = genInstanceRequirement(builder, dependent,
                                               numDependents, capsules);
    }

    // Now that we have the full size of the vector, allocate an array of
    // sufficient size to accommodate all the required instances.
    llvm::BasicBlock *initBB = llvm::BasicBlock::Create("init", ctor, constructBB);
    builder.SetInsertPoint(initBB);

    llvm::Value *size =
        llvm::ConstantInt::get(llvm::Type::Int32Ty, numDependents);
    capsules = builder.CreateMalloc(DomainInstancePtrTy, size);
    llvm::Value *dst = builder.CreateStructGEP(instance, 4);
    builder.CreateStore(capsules, dst);
    builder.CreateBr(constructBB);

    // Generate a return.
    builder.SetInsertPoint(constructBB);
    builder.CreateRetVoid();

    return ctor;
}

// Helper method for genCapsuleCtor.  Generates a call to get_domain for the
// given instance decl, storing the result into the destination vector with the
// given index.
unsigned CommaRT::genInstanceRequirement(llvm::IRBuilder<> &builder,
                                         DomainInstanceDecl *instanceDecl,
                                         unsigned index,
                                         llvm::Value *destVector)
{
    if (DomainDecl *domain = instanceDecl->getDefiningDomain()) {
        // The instance is not parameterized.  Simply call get_domain directly.
        llvm::GlobalValue *info = CG.lookupCapsuleInfo(domain);
        assert(info && "Could not resolve capsule info!");

        llvm::Value *instance = getDomain(builder, info);
        llvm::Value *slotIndex = llvm::ConstantInt::get(llvm::Type::Int32Ty, index);
        builder.CreateStore(instance, builder.CreateGEP(destVector, slotIndex));

        index++;
    }
    else {
        assert(false && "Cannot codegen parameterized dependents yet!");
    }

    return index;
}

// Generates an llvm IR integer constant representing the number of
// supersignatures implemented by the given capsule.
llvm::ConstantInt *CommaRT::genSignatureCount(CodeGenCapsule &CGC)
{
    Domoid *theCapsule = CGC.getCapsule();
    SignatureSet &sigSet = theCapsule->getSignatureSet();
    const llvm::Type *intTy = DIType.getFieldType(DomainInfoTy::NumSigs);

    return llvm::ConstantInt::get(intTy, sigSet.numSignatures());
}

llvm::Constant *CommaRT::genSignatureOffsets(CodeGenCapsule &CGC)
{
    llvm::Module *M = CG.getModule();
    Domoid *theCapsule = CGC.getCapsule();
    SignatureSet &sigSet = theCapsule->getSignatureSet();
    const llvm::PointerType *vectorTy =
        cast<llvm::PointerType>(DIType.getFieldType(DomainInfoTy::SigOffsets));
    const llvm::Type *elemTy = vectorTy->getElementType();

    // If the domain in question does not implement any signatures, generate a
    // null for the offset vector.
    if (!sigSet.numSignatures())
        return llvm::ConstantPointerNull::get(vectorTy);

    // Otherwise, collect all of the offsets.
    typedef SignatureSet::const_iterator iterator;
    std::vector<llvm::Constant *> offsets;
    unsigned index = 0;

    for (iterator iter = sigSet.beginDirect();
         iter != sigSet.endDirect(); ++iter)
        index = emitOffsetsForSignature(*iter, index, elemTy, offsets);

    llvm::ArrayType *arrayTy = llvm::ArrayType::get(elemTy, offsets.size());
    llvm::Constant *offsetInit = llvm::ConstantArray::get(arrayTy, offsets);
    llvm::GlobalVariable *offsetVal =
        new llvm::GlobalVariable(arrayTy, true,
                                 llvm::GlobalVariable::InternalLinkage,
                                 offsetInit, "", M);
    return llvm::ConstantExpr::getPointerCast(offsetVal, vectorTy);
}

unsigned
CommaRT::emitOffsetsForSignature(SignatureType *sig,
                                 unsigned index,
                                 const llvm::Type *elemTy,
                                 std::vector<llvm::Constant *> &offsets)
{
    typedef SignatureSet::const_iterator sig_iterator;
    typedef DeclRegion::ConstDeclIter decl_iterator;

    const Sigoid *sigoid = sig->getSigoid();

    offsets.push_back(llvm::ConstantInt::get(elemTy, index));

    for (decl_iterator iter = sigoid->beginDecls();
         iter != sigoid->endDecls(); ++iter) {
        const SubroutineDecl *decl = dyn_cast<SubroutineDecl>(*iter);
        if (decl && decl->isImmediate())
                index++;
    }

    const SignatureSet &sigSet = sigoid->getSignatureSet();
    for (sig_iterator iter = sigSet.beginDirect();
         iter != sigSet.endDirect(); ++iter)
        index = emitOffsetsForSignature(*iter, index, elemTy, offsets);

    return index;
}
