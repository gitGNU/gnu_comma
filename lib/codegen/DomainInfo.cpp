//===-- codegen/DomainInfo.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "CodeGenCapsule.h"
#include "DependencySet.h"
#include "DomainInfo.h"
#include "DomainInstance.h"
#include "comma/ast/SignatureSet.h"
#include "comma/codegen/CommaRT.h"
#include "comma/codegen/Mangle.h"

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
      theType(CG.getOpaqueTy()) { }

void DomainInfo::init()
{
    std::vector<const llvm::Type*> members;

    members.push_back(CG.getInt32Ty());
    members.push_back(CG.getPointerType(CG.getInt8Ty()));
    members.push_back(CRT.getType<CommaRT::CRT_DomainCtor>());
    members.push_back(CRT.getType<CommaRT::CRT_ITable>());

    llvm::StructType *InfoTy = CG.getStructTy(members);
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

    ctorTy = llvm::FunctionType::get(CG.getVoidTy(), args, false);
    return CG.getPointerType(ctorTy);
}

llvm::GlobalVariable *DomainInfo::emit(const Domoid *domoid)
{
    std::vector<llvm::Constant *> elts;

    elts.push_back(genArity(domoid));
    elts.push_back(genName(domoid));
    elts.push_back(genConstructor(domoid));
    elts.push_back(genITable(domoid));

    llvm::Constant *theInfo = llvm::ConstantStruct::get(getType(), elts);
    return CG.makeExternGlobal(theInfo, false, mangle::getLinkName(domoid));
}

std::string DomainInfo::getLinkName(const Domoid *domoid)
{
    return mangle::getLinkName(domoid) + "__0domain_info";
}

std::string DomainInfo::getCtorName(const Domoid *domoid)
{
    return mangle::getLinkName(domoid) + "__0ctor";
}

/// Allocates a constant string for a domain_info's name.
llvm::Constant *DomainInfo::genName(const Domoid *domoid)
{
    const llvm::PointerType *NameTy = getFieldType<Name>();

    llvm::Constant *capsuleName = CG.emitInternString(domoid->getString());
    return CG.getPointerCast(capsuleName, NameTy);
}

/// Generates the arity for an instance.
llvm::Constant *DomainInfo::genArity(const Domoid *domoid)
{
    const llvm::IntegerType *ArityTy = getFieldType<Arity>();

    if (const FunctorDecl *functor = dyn_cast<FunctorDecl>(domoid))
        return llvm::ConstantInt::get(ArityTy, functor->getArity());
    else
        return llvm::ConstantInt::get(ArityTy, 0);
}

/// Generates a constructor function for an instance.
llvm::Constant *DomainInfo::genConstructor(const Domoid *domoid)
{
    const DependencySet &DS = CG.getDependencySet(domoid);

    // If the capsule in question does not have any dependencies, do not build a
    // function -- just return 0.  The runtime will not call thru null
    // constructors.
    if (DS.size() == 0)
        return CG.getNullPointer(getFieldType<Ctor>());

    std::string ctorName = getCtorName(domoid);
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
    llvm::BasicBlock *constructBB = CG.makeBasicBlock("construct", ctor);
    llvm::IRBuilder<> builder(CG.getLLVMContext());
    builder.SetInsertPoint(constructBB);

    // The first (and only) argument of the constructor is a domain_instance_t.
    llvm::Value *instance = &(ctor->getArgumentList().front());

    // Extract a pointer to the "required capsules" array.
    llvm::Value *capsules =
        CRT.getDomainInstance()->loadLocalVec(builder, instance);

    // Iterate over the set of capsule dependencies and emit calls to
    // get_domain for each, keeping track of the number of dependents.
    typedef DependencySet::iterator iterator;
    iterator E = DS.end();
    for (iterator I = DS.begin(); I != E; ++I) {
        unsigned ID = DS.getDependentID(I);
        genInstanceRequirement(builder, DS, ID, capsules, instance);
    }

    // Now that we have the full size of the vector, allocate an array of
    // sufficient size to accommodate all the required instances.
    llvm::BasicBlock *initBB = CG.makeBasicBlock("init", ctor, constructBB);
    builder.SetInsertPoint(initBB);

    llvm::Value *size = llvm::ConstantInt::get(CG.getInt32Ty(), DS.size());
    capsules = builder.CreateMalloc(CRT.getType<CommaRT::CRT_DomainInstance>(), size);
    CRT.getDomainInstance()->setLocalVec(builder, instance, capsules);
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
                                        const DependencySet &DS,
                                        unsigned ID,
                                        llvm::Value *destVector,
                                        llvm::Value *percent)
{
    const DomainInstanceDecl *instance = DS.getDependent(ID);
    const Domoid *domoid = instance->getDefinition();

    if (isa<DomainDecl>(domoid))
        genDomainRequirement(builder, DS, ID, destVector);
    else
        genFunctorRequirement(builder, DS, ID, destVector, percent);
}

/// \brief Helper method for genInstanceRequirement.
///
/// Constructs the dependency info for the dependency represented by \p ID,
/// which must be a non-parameterized domain.
void DomainInfo::genDomainRequirement(llvm::IRBuilder<> &builder,
                                      const DependencySet &DS,
                                      unsigned ID,
                                      llvm::Value *destVector)
{
    const DomainInstanceDecl *instance = DS.getDependent(ID);
    const DomainDecl *domain = instance->getDefiningDomain();
    assert(domain && "Cannot gen requirement for this type of instance!");

    llvm::GlobalValue *info = CG.lookupCapsuleInfo(domain);
    assert(info && "Could not resolve capsule info!");

    llvm::Value *ptr = CRT.getDomain(builder, info);
    llvm::Value *slotIndex = llvm::ConstantInt::get(CG.getInt32Ty(), ID);
    builder.CreateStore(ptr, builder.CreateGEP(destVector, slotIndex));
}

/// \brief Helper method for genInstanceRequirement.
///
/// Constructs the dependency info for the dependency represented by \p ID,
/// which must be a parameterized domain (functor).
void DomainInfo::genFunctorRequirement(llvm::IRBuilder<> &builder,
                                       const DependencySet &DS,
                                       unsigned ID,
                                       llvm::Value *destVector,
                                       llvm::Value *percent)
{
    const DomainInstanceDecl *instance = DS.getDependent(ID);
    const FunctorDecl *functor = instance->getDefiningFunctor();
    assert(functor && "Cannot gen requirement for this type of instance!");

    const DomainInstance *DInstance = CRT.getDomainInstance();

    llvm::Value *info = CG.lookupCapsuleInfo(functor);
    if (!info) {
        // The only case where the lookup can fail is when the functor being
        // applied is the current capsule for which we are generating a
        // constructor for.  Fortunately, the runtime provides this info via
        // this instances percent node.
        assert(functor == cast<FunctorDecl>(DS.getCapsule()) &&
               "Could not resolve capsule info!");
        info = DInstance->loadInfo(builder, percent);
    }

    std::vector<llvm::Value *> arguments;
    arguments.push_back(info);

    for (unsigned i = 0; i < instance->getArity(); ++i) {
        DomainType *argTy = cast<DomainType>(instance->getActualParamType(i));

        if (PercentDecl *pdecl = argTy->getPercentDecl()) {
            assert(pdecl->getDefinition() == DS.getCapsule() &&
                   "Percent node does not represent the current domain!");

            // The argument to this functor is %. Simply push the given percent
            // value onto get_domains argument list.
            arguments.push_back(percent);
        }
        else if (DomainInstanceDecl *arg = argTy->getInstanceDecl()) {
            DependencySet::iterator argPos = DS.find(arg);
            assert(argPos != DS.end() && "Dependency lookup failed!");
            unsigned argIndex = DS.getDependentID(argPos);

            // Load the instance from the destination vector and push it onto
            // the argument list.
            llvm::Value *instanceSlot =
                llvm::ConstantInt::get(CG.getInt32Ty(), argIndex);
            llvm::Value *argInstance =
                builder.CreateLoad(builder.CreateGEP(destVector, instanceSlot));
            arguments.push_back(argInstance);
        }
        else {
            AbstractDomainDecl *arg = argTy->getAbstractDecl();
            unsigned paramIdx = DS.getCapsule()->getFormalIndex(arg);

            // Load the instance corresponding to the formal parameter and push
            // as an argument.
            llvm::Value *param = DInstance->loadParam(builder, percent, paramIdx);
            arguments.push_back(param);
        }
    }

    llvm::Value *theInstance = CRT.getDomain(builder, arguments);
    llvm::Value *slotIndex = llvm::ConstantInt::get(CG.getInt32Ty(), ID);
    builder.CreateStore(theInstance, builder.CreateGEP(destVector, slotIndex));
}


/// Generates a pointer to the instance table for an instance.
llvm::Constant *DomainInfo::genITable(const Domoid *domoid)
{
    // Initially the instance table is always null.  A table is allocated at
    // runtime when needed.
    return CG.getNullPointer(getFieldType<ITable>());
}

