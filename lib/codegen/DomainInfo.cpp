//===-- codegen/DomainInfo.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "ExportMap.h"
#include "DomainInfo.h"
#include "DomainInstance.h"
#include "DomainView.h"
#include "comma/ast/SignatureSet.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/ADT/IndexedMap.h"
#include "llvm/Support/Casting.h"
#include "llvm/DerivedTypes.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DomainInfo::DomainInfo(CommaRT &CRT)
    : CRT(CRT),
      CG(CRT.getCodeGen()),
      TD(CG.getTargetData()),
      theType(llvm::OpaqueType::get()) { }

void DomainInfo::init()
{
    std::vector<const llvm::Type*> members;
    const llvm::Type* IntPtrTy = TD.getIntPtrType();

    members.push_back(llvm::Type::Int32Ty);
    members.push_back(llvm::Type::Int32Ty);
    members.push_back(CG.getPointerType(llvm::Type::Int8Ty));
    members.push_back(CRT.getType<CommaRT::CRT_DomainCtor>());
    members.push_back(CRT.getType<CommaRT::CRT_ITable>());
    members.push_back(CG.getPointerType(IntPtrTy));
    members.push_back(CG.getPointerType(CRT.getType<CommaRT::CRT_ExportFn>()));

    llvm::StructType *InfoTy = llvm::StructType::get(members);
    cast<llvm::OpaqueType>(theType.get())->refineAbstractTypeTo(InfoTy);
}

const std::string DomainInfo::theTypeName("comma_domain_info_t");

const llvm::StructType *DomainInfo::getType() const
{
    return cast<llvm::StructType>(theType.get());
}

const llvm::PointerType *DomainInfo::getPointerTypeTo() const
{
    return llvm::PointerType::getUnqual(theType.get());
}

const llvm::PointerType *DomainInfo::getCtorPtrType() const
{
    std::vector<const llvm::Type*> args;
    const llvm::Type *ctorTy;

    args.push_back(CRT.getType<CommaRT::CRT_DomainInstance>());

    ctorTy = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);
    return CG.getPointerType(ctorTy);
}

llvm::GlobalVariable *DomainInfo::generateInstance(CodeGenCapsule &CGC)
{
    std::vector<llvm::Constant *> elts;

    elts.push_back(genArity(CGC));
    elts.push_back(genSignatureCount(CGC));
    elts.push_back(genName(CGC));
    elts.push_back(genConstructor(CGC));
    elts.push_back(genITable(CGC));
    elts.push_back(genSignatureOffsets(CGC));
    elts.push_back(genExportArray(CGC));

    llvm::Constant *theInfo = llvm::ConstantStruct::get(getType(), elts);
    return CG.makeExternGlobal(theInfo, false, getLinkName(CGC));
}

std::string DomainInfo::getLinkName(const CodeGenCapsule &CGC) const
{
    return CGC.getLinkName() + "__0domain_info";
}

/// Allocates a constant string for a domain_info's name.
llvm::Constant *DomainInfo::genName(CodeGenCapsule &CGC)
{
    Domoid *theCapsule = CGC.getCapsule();
    const llvm::PointerType *NameTy = getFieldType<Name>();

    llvm::Constant *capsuleName = CG.emitStringLiteral(theCapsule->getString());
    return CG.getPointerCast(capsuleName, NameTy);
}

/// Generates the arity for an instance.
llvm::Constant *DomainInfo::genArity(CodeGenCapsule &CGC)
{
    Domoid *theCapsule = CGC.getCapsule();
    const llvm::IntegerType *ArityTy = getFieldType<Arity>();

    if (FunctorDecl *functor = dyn_cast<FunctorDecl>(theCapsule))
        return llvm::ConstantInt::get(ArityTy, functor->getArity());
    else
        return llvm::ConstantInt::get(ArityTy, 0);
}

/// Generates the signature count for an instance.
llvm::Constant *DomainInfo::genSignatureCount(CodeGenCapsule &CGC)
{
    Domoid *theCapsule = CGC.getCapsule();
    SignatureSet &sigSet = theCapsule->getSignatureSet();
    const llvm::Type *intTy = getFieldType<NumSigs>();

    return llvm::ConstantInt::get(intTy, sigSet.numSignatures());
}

/// Generates a constructor function for an instance.
llvm::Constant *DomainInfo::genConstructor(CodeGenCapsule &CGC)
{
    // If the capsule in question does not have any dependencies, do not build a
    // function -- just return 0.  The runtime will not call thru null
    // constructors.
    if (CGC.dependencyCount() == 0)
        return CG.getNullPointer(getFieldType<Ctor>());

    std::string ctorName = CGC.getLinkName() + "__0ctor";
    const llvm::FunctionType *ctorTy;
    llvm::Function *ctor;

    ctorTy = cast<llvm::FunctionType>(
        CRT.getType<CommaRT::CRT_DomainCtor>()->getElementType());
    ctor = CG.makeInternFunction(ctorTy, ctorName);

    CG.insertGlobal(ctorName, ctor);

    // Create a basic block to hold a sequence of get_domain calls which
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
    // get_domain for each, keeping track of the number of dependents.
    unsigned numDependents = CGC.dependencyCount();
    for (unsigned ID = 1; ID <= numDependents; ++ID)
        genInstanceRequirement(builder, CGC, ID, capsules, instance);

    // Now that we have the full size of the vector, allocate an array of
    // sufficient size to accommodate all the required instances.
    llvm::BasicBlock *initBB = llvm::BasicBlock::Create("init", ctor, constructBB);
    builder.SetInsertPoint(initBB);

    llvm::Value *size =
        llvm::ConstantInt::get(llvm::Type::Int32Ty, numDependents);
    capsules = builder.CreateMalloc(CRT.getType<CommaRT::CRT_DomainInstance>(), size);
    llvm::Value *dst = builder.CreateStructGEP(instance, 4);
    builder.CreateStore(capsules, dst);
    builder.CreateBr(constructBB);

    // Generate a return.
    builder.SetInsertPoint(constructBB);
    builder.CreateRetVoid();

    return ctor;
}

/// \brief Helper method for genConstructor.
///
/// Generates a call to get_domain for the capsule dependency represented by \p
/// ID.  The dependency (and any outstanding sub-dependents) are consecutively
/// stored into \p destVector beginning at an index derived from \p ID.  \p
/// percent represents the domain_instance serving as argument to the
/// constructor.
void DomainInfo::genInstanceRequirement(llvm::IRBuilder<> &builder,
                                        CodeGenCapsule &CGC,
                                        unsigned ID,
                                        llvm::Value *destVector,
                                        llvm::Value *percent)
{
    DomainInstanceDecl *instance = CGC.getDependency(ID);
    Domoid *domoid = instance->getDefiningDecl();

    if (isa<DomainDecl>(domoid))
        genDomainRequirement(builder, CGC, ID, destVector);
    else
        genFunctorRequirement(builder, CGC, ID, destVector, percent);
}

/// \brief Helper method for genInstanceRequirement.
///
/// Constructs the dependency info for the dependency represented by \p ID,
/// which must be a non-parameterized domain.
void DomainInfo::genDomainRequirement(llvm::IRBuilder<> &builder,
                                      CodeGenCapsule &CGC,
                                      unsigned ID,
                                      llvm::Value *destVector)
{
    DomainInstanceDecl *instance = CGC.getDependency(ID);
    DomainDecl *domain = instance->getDefiningDomain();
    assert(domain && "Cannot gen requirement for this type of instance!");

    llvm::GlobalValue *info = CG.lookupCapsuleInfo(domain);
    assert(info && "Could not resolve capsule info!");

    llvm::Value *ptr = CRT.getDomain(builder, info);
    llvm::Value *slotIndex = llvm::ConstantInt::get(llvm::Type::Int32Ty, ID - 1);
    builder.CreateStore(ptr, builder.CreateGEP(destVector, slotIndex));
}

/// \brief Helper method for genInstanceRequirement.
///
/// Constructs the dependency info for the dependency represented by \p ID,
/// which must be a parameterized domain (functor).
void DomainInfo::genFunctorRequirement(llvm::IRBuilder<> &builder,
                                       CodeGenCapsule &CGC,
                                       unsigned ID,
                                       llvm::Value *destVector,
                                       llvm::Value *percent)
{
    DomainInstanceDecl *instance = CGC.getDependency(ID);
    FunctorDecl *functor = instance->getDefiningFunctor();
    assert(functor && "Cannot gen requirement for this type of instance!");

    llvm::GlobalValue *info = CG.lookupCapsuleInfo(functor);
    assert(info && "Could not resolve capsule info!");

    std::vector<llvm::Value *> arguments;
    arguments.push_back(info);

    const DomainInstance *DInstance = CRT.getDomainInstance();
    const DomainView *DView = CRT.getDomainView();

    for (unsigned i = 0; i < instance->getArity(); ++i) {
        DomainType *argTy = cast<DomainType>(instance->getActualParameter(i));
        SignatureType *targetTy = functor->getFormalSignature(i);

        if (DomainInstanceDecl *arg = argTy->getInstanceDecl()) {
            unsigned argIndex = CGC.getDependencyID(arg) - 1;

            // Load the instance from the destination vector.
            llvm::Value *instanceSlot =
                llvm::ConstantInt::get(llvm::Type::Int32Ty, argIndex);
            llvm::Value *argInstance =
                builder.CreateLoad(builder.CreateGEP(destVector, instanceSlot));

            // Lookup the index of the target signature wrt the argument domain,
            // load the associated view, and push it onto get_domains argument
            // list.
            unsigned sigOffset = CRT.getSignatureOffset(arg, targetTy);
            llvm::Value *view =
                DInstance->loadView(builder, argInstance, sigOffset);
            arguments.push_back(view);
        }
        else {
            AbstractDomainDecl *arg = argTy->getAbstractDecl();

            unsigned paramIdx = functor->getFormalIndex(arg);
            SignatureType *targetTy = functor->getFormalSignature(paramIdx);
            unsigned sigOffset = CRT.getSignatureOffset(arg, targetTy);

            // Load the view corresponding to the formal parameter.
            llvm::Value *paramView =
                DInstance->loadParam(builder, percent, paramIdx);

            // Downcast the view to match that of the required signature and
            // supply it to the get_domain call.
            llvm::Value *argView =
                DView->downcast(builder, paramView, sigOffset);

            arguments.push_back(argView);
        }
    }

    llvm::Value *theInstance = CRT.getDomain(builder, arguments);
    llvm::Value *slotIndex = llvm::ConstantInt::get(llvm::Type::Int32Ty, ID - 1);
    builder.CreateStore(theInstance, builder.CreateGEP(destVector, slotIndex));
}


/// Generates a pointer to the instance table for an instance.
llvm::Constant *DomainInfo::genITable(CodeGenCapsule &CGC)
{
    // Initially the instance table is always null.  A table is allocated at
    // runtime when needed.
    return CG.getNullPointer(getFieldType<ITable>());
}

/// Generates the signature offset vector for an instance.
llvm::Constant *DomainInfo::genSignatureOffsets(CodeGenCapsule &CGC)
{
    Domoid *theCapsule = CGC.getCapsule();
    SignatureSet &sigSet = theCapsule->getSignatureSet();
    const llvm::PointerType *vectorTy = getFieldType<SigOffsets>();
    const llvm::Type *elemTy = vectorTy->getElementType();

    // If the domain in question does not implement a signature, generate a null
    // for the offset vector.
    if (!sigSet.numSignatures())
        return CG.getNullPointer(vectorTy);

    // Otherwise, collect all of the offsets.
    typedef SignatureSet::const_iterator iterator;
    std::vector<llvm::Constant *> offsets;
    unsigned index = 0;

    for (iterator iter = sigSet.beginDirect();
         iter != sigSet.endDirect(); ++iter)
        index = getOffsetsForSignature(*iter, index, offsets);

    llvm::Constant *offsetInit = CG.getConstantArray(elemTy, offsets);
    llvm::GlobalVariable *offsetVal = CG.makeInternGlobal(offsetInit, true);
    return CG.getPointerCast(offsetVal, vectorTy);
}

/// \brief Helper method for genSignatureOffsets.
///
/// Populates the given vector \p offsets with constant indexes, representing
/// the offsets required to appear in a domain_info's signature_offset field.
/// Each of the offsets is adjusted by \p index.
unsigned
DomainInfo::getOffsetsForSignature(SignatureType *sig,
                                   unsigned index,
                                   std::vector<llvm::Constant *> &offsets)
{
    typedef SignatureSet::const_iterator sig_iterator;
    typedef DeclRegion::ConstDeclIter decl_iterator;

    const Sigoid *sigoid = sig->getSigoid();
    ExportMap::SignatureKey key = CRT.getExportMap().lookupSignature(sigoid);
    const llvm::Type *elemTy = getFieldType<SigOffsets>()->getElementType();

    for (ExportMap::offset_iterator iter = ExportMap::begin_offsets(key);
         iter != ExportMap::end_offsets(key); ++iter) {
        unsigned i = index + *iter;
        offsets.push_back(llvm::ConstantInt::get(elemTy, i));
    }

    return index + ExportMap::numberOfExports(key);
}

/// Generates the export array for an instance.
llvm::Constant *DomainInfo::genExportArray(CodeGenCapsule &CGC)
{
    Domoid *theCapsule = CGC.getCapsule();
    const llvm::PointerType *exportVecTy = getFieldType<Exvec>();
    const llvm::PointerType *exportFnTy =
        cast<llvm::PointerType>(exportVecTy->getElementType());
    llvm::IndexedMap<llvm::Constant *> exportMap;

    for (Domoid::ConstDeclIter iter = theCapsule->beginDecls();
         iter != theCapsule->endDecls(); ++iter) {
        const SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(*iter);
        if (srDecl && !srDecl->isImmediate()) {
            std::string linkName = CodeGen::getLinkName(srDecl);
            llvm::GlobalValue *fn = CG.lookupGlobal(linkName);
            unsigned index = CRT.getExportMap().getIndex(srDecl);

            assert(fn && "Export lookup failed!");
            exportMap.grow(index);
            exportMap[index] = fn;
        }
    }

    // The exportMap is now ordered wrt the export indexes.  Produce a constant
    // llvm array containing the exports, each cast to a generic function
    // pointer type.
    std::vector<llvm::Constant *> ptrs;
    for (unsigned i = 0; i < exportMap.size(); ++i) {
        llvm::Constant *fn = exportMap[i];
        assert(fn && "Empty entry in export map!");
        ptrs.push_back(CG.getPointerCast(fn, exportFnTy));
    }

    llvm::Constant *exportInit = CG.getConstantArray(exportFnTy, ptrs);
    llvm::GlobalVariable *exportVec = CG.makeInternGlobal(exportInit, true);
    return CG.getPointerCast(exportVec, exportVecTy);
}


/// Loads the offset at the given index from a domain_info's signature offset
/// vector.
llvm::Value *DomainInfo::indexSigOffset(llvm::IRBuilder<> &builder,
                                        llvm::Value *DInfo,
                                        llvm::Value *index) const
{
    assert(DInfo->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *offVecAdr = builder.CreateStructGEP(DInfo, SigOffsets);
    llvm::Value *offVec = builder.CreateLoad(offVecAdr);
    llvm::Value *offAdr = builder.CreateGEP(offVec, index);
    return builder.CreateLoad(offAdr);
}


/// Loads a pointer to the first element of the export vector associated
/// with the given domain_info object.
llvm::Value *DomainInfo::loadExportVec(llvm::IRBuilder<> &builder,
                                       llvm::Value *DInfo) const
{
    assert(DInfo->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *exportsAddr = builder.CreateStructGEP(DInfo, Exvec);
    return builder.CreateLoad(exportsAddr);
}

/// Indexes into the export vector and loads a pointer to the associated export.
llvm::Value *DomainInfo::loadExportFn(llvm::IRBuilder<> &builder,
                                      llvm::Value *DInfo,
                                      llvm::Value *exportIdx) const
{
    assert(DInfo->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *exportVec = loadExportVec(builder, DInfo);
    llvm::Value *exportAddr = builder.CreateGEP(exportVec, exportIdx);
    return builder.CreateLoad(exportAddr);
}
