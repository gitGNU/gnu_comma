//===-- codegen/CommaRT.cpp ----------------------------------- -*- C++ -*-===//
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
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/ADT/IndexedMap.h"
#include "llvm/Module.h"
#include "llvm/Value.h"
#include "llvm/Support/IRBuilder.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

CommaRT::CommaRT(CodeGen &CG)
    : CG(CG),
      EM(new ExportMap),
      ITableName("_comma_itable_t"),
      DomainCtorName("_comma_domain_ctor_t"),

      DInfo(0),
      DomainInfoPtrTy(0),

      DView(0),
      DomainViewPtrTy(0),

      DInstance(0),
      DomainInstancePtrTy(0),

      ExportFnPtrTy(getExportFnPtrTy()),
      ITablePtrTy(getITablePtrTy()),
      DomainCtorPtrTy(0),

      GetDomainName("_comma_get_domain")
{
    DInfo = new DomainInfo(*this);
    DomainInfoPtrTy = DInfo->getPointerTypeTo();

    DView = new DomainView(*this);
    DomainViewPtrTy = DView->getPointerTypeTo();

    DInstance = new DomainInstance(*this);
    DomainInstancePtrTy = DInstance->getPointerTypeTo();

    DomainCtorPtrTy = DInfo->getCtorPtrType();

    DInfo->init();
    DView->init();
    DInstance->init();

    generateRuntimeTypes();
    generateRuntimeFunctions();
}

CommaRT::~CommaRT()
{
    delete EM;
    delete DInfo;
    delete DView;
}

const std::string &CommaRT::getTypeName(TypeId id) const
{
    switch (id) {
    default:
        assert(false && "Invalid type id!");
        return InvalidName;
    case CRT_ITable:
        return ITableName;
    case CRT_DomainInfo:
        return DInfo->getTypeName();
    case CRT_DomainView:
        return DView->getTypeName();
    case CRT_DomainInstance:
        return DInstance->getTypeName();
    case CRT_DomainCtor:
        return DomainCtorName;
    }
}

void CommaRT::generateRuntimeTypes()
{
    // Define the types within the Module.
    llvm::Module *M = CG.getModule();
    M->addTypeName(getTypeName(CRT_DomainInfo),     getType<CRT_DomainInfo>());
    M->addTypeName(getTypeName(CRT_DomainView),     getType<CRT_DomainView>());
    M->addTypeName(getTypeName(CRT_DomainInstance), getType<CRT_DomainInstance>());
}

const llvm::PointerType *CommaRT::getDomainCtorPtrTy()
{
    std::vector<const llvm::Type*> args;

    args.push_back(DomainInstancePtrTy);

    const llvm::Type *ctorTy = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);
    return CG.getPointerType(ctorTy);
}

const llvm::PointerType *CommaRT::getExportFnPtrTy()
{
    std::vector<const llvm::Type*> args;
    llvm::Type *ftype;

    ftype = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);
    return CG.getPointerType(ftype);
}

const llvm::PointerType *CommaRT::getITablePtrTy()
{
    return CG.getPointerType(llvm::Type::Int8Ty);
}

void CommaRT::generateRuntimeFunctions()
{
    defineGetDomain();
}

// Builds a declaration in LLVM IR for the get_domain runtime function.
void CommaRT::defineGetDomain()
{
    const llvm::Type *retTy = getType<CRT_DomainInstance>();
    std::vector<const llvm::Type *> args;

    args.push_back(getType<CRT_DomainInfo>());

    // get_domain takes a pointer to a domain_instance_t as first argument, and
    // then a variable number of domain_view_t args corresponding to the
    // parameters.
    llvm::FunctionType *fnTy = llvm::FunctionType::get(retTy, args, true);

    getDomainFn = CG.makeFunction(fnTy, GetDomainName);
}

void CommaRT::registerSignature(const Sigoid *sigoid)
{
    EM->addSignature(sigoid);
}

llvm::GlobalVariable *CommaRT::registerCapsule(CodeGenCapsule &CGC)
{
    return DInfo->generateInstance(CGC);
}

llvm::Value *CommaRT::getDomain(llvm::IRBuilder<> &builder,
                                llvm::GlobalValue *capsuleInfo) const
{
    return builder.CreateCall(getDomainFn, capsuleInfo);
}

llvm::Value *CommaRT::getDomain(llvm::IRBuilder<> &builder,
                                std::vector<llvm::Value *> &args) const
{
    assert(args.front()->getType() == getType<CRT_DomainInfo>()
           && "First argument is not a domain_info_t!");
    return builder.CreateCall(getDomainFn, args.begin(), args.end());
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
                            llvm::ConstantInt::get(llvm::Type::Int32Ty, ID - 1));

    // And finally load the indexed pointer, yielding the local capsule.
    return builder.CreateLoad(elt);
}

unsigned CommaRT::getSignatureOffset(Domoid *domoid, SignatureType *target)
{
    typedef SignatureSet::iterator iterator;
    SignatureSet &SS = domoid->getSignatureSet();

    unsigned offset = 0;
    for (iterator iter = SS.begin(); iter != SS.end(); ++iter) {
        if (target->equals(*iter))
            return offset;
        offset++;
    }
    assert(false && "Could not find target signature!");
    return 0;
}

llvm::Value *CommaRT::genAbstractCall(llvm::IRBuilder<> &builder,
                                      llvm::Value *percent,
                                      const FunctionDecl *fdecl,
                                      const std::vector<llvm::Value *> &args) const
{
    const AbstractDomainDecl *param;
    const FunctorDecl *context;
    SignatureType *target;

    param   = cast<AbstractDomainDecl>(fdecl->getDeclRegion());
    context = cast<FunctorDecl>(param->getDeclRegion());
    target  = param->getSignatureType();

    unsigned paramIdx = context->getFormalIndex(param);

    // Get the index of the function we wish to call by first resolving the
    // offset for the signature of origin of the function decl, and the
    // export offset within that signature.
    unsigned sigIdx = ExportMap::getSignatureOffset(fdecl);
    unsigned exportIdx = EM->getLocalIndex(fdecl);

    // Obtain the domain_view_t corresponding to this parameter.
    llvm::Value *view = DInstance->loadParam(builder, percent, paramIdx);

    // Add the signature offset to the views offset, yielding the DFPO index of
    // the signature.
    llvm::Value *viewIndexAdr = builder.CreateStructGEP(view, 1);
    llvm::Value *viewIndex = builder.CreateLoad(viewIndexAdr);
    llvm::Value *sigIndex =
        builder.CreateAdd(viewIndex,
                          llvm::ConstantInt::get(llvm::Type::Int64Ty, sigIdx));

    // Index into the views offset vector to get the start of the target
    // signatures export region.  Add to that the export index and load the
    // needed function.
    llvm::Value *abstractInstance = DView->loadInstance(builder, view);
    llvm::Value *abstractInfo = DInstance->loadInfo(builder, abstractInstance);
    llvm::Value *sigOffset = DInfo->indexSigOffset(builder, abstractInfo, sigIndex);
    llvm::Value *exportOffset =
        builder.CreateAdd(sigOffset,
                          llvm::ConstantInt::get(llvm::Type::Int64Ty, exportIdx));
    llvm::Value *exportFn = DInfo->loadExportFn(builder, abstractInfo, exportOffset);

    // With the export in hand, bit cast it to a function of the required type.
    CodeGenTypes &CGT = CG.getTypeGenerator();
    const llvm::FunctionType *funcTy = CGT.lowerType(fdecl->getType());
    llvm::PointerType *funcPtrTy = CG.getPointerType(funcTy);
    llvm::Value *func = builder.CreateBitCast(exportFn, funcPtrTy);

    // Finally, create and return the call.
    std::vector<llvm::Value *> arguments;
    arguments.push_back(abstractInstance);
    arguments.insert(arguments.end(), args.begin(), args.end());
    return builder.CreateCall(func, arguments.begin(), arguments.end());
}
