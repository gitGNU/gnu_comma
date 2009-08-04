//===-- codegen/DomainView.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DomainInstance.h"
#include "DomainView.h"
#include "comma/codegen/CommaRT.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DomainView::DomainView(CommaRT &CRT)
    : CRT(CRT),
      CG(CRT.getCodeGen()),
      TD(CG.getTargetData()),
      theType(llvm::OpaqueType::get()) { }

void DomainView::init()
{
    std::vector<const llvm::Type*> members;
    const llvm::Type* IntPtrTy = TD.getIntPtrType();

    members.push_back(CRT.getType<CommaRT::CRT_DomainInstance>());
    members.push_back(IntPtrTy);

    llvm::StructType *ViewTy = llvm::StructType::get(members);
    cast<llvm::OpaqueType>(theType.get())->refineAbstractTypeTo(ViewTy);
}

const std::string DomainView::theTypeName("comma_domain_view_t");

const llvm::StructType *DomainView::getType() const
{
    return cast<llvm::StructType>(theType.get());
}

const llvm::PointerType *DomainView::getPointerTypeTo() const
{
    return llvm::PointerType::getUnqual(theType.get());
}

/// Loads the domain_instance associated with the given domain_view.
llvm::Value *DomainView::loadInstance(llvm::IRBuilder<> &builder,
                                      llvm::Value *DView) const
{
    assert(DView->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *instanceAddr = builder.CreateStructGEP(DView, Instance);
    return builder.CreateLoad(instanceAddr);
}

/// Loads the signature index associated with the given domain_view.
llvm::Value *DomainView::loadIndex(llvm::IRBuilder<> &builder,
                                   llvm::Value *view) const
{
    assert(view->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    llvm::Value *indexAddr = builder.CreateStructGEP(view, Index);
    return builder.CreateLoad(indexAddr);
}


/// Downcasts the provided domain_view to another view corresponding to the
/// signature with the given (view-relative) index.
llvm::Value *DomainView::downcast(llvm::IRBuilder<> &builder,
                                  llvm::Value *view,
                                  unsigned sigIndex) const
{
    assert(view->getType() == getPointerTypeTo() &&
           "Wrong type of LLVM Value!");

    // A zero index means that the "principle signature" of the supplied view is
    // required.  In that case, just return the given view.
    if (sigIndex == 0)
        return view;

    const DomainInstance *DInstance = CRT.getDomainInstance();

    llvm::Value *sigIdx = llvm::ConstantInt::get(llvm::Type::Int32Ty, sigIndex);

    llvm::Value *instance = loadInstance(builder, view);
    llvm::Value *index = builder.CreateAdd(loadIndex(builder, view), sigIdx);
    return DInstance->loadView(builder, instance, index);
}

