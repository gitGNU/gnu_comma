//===-- codegen/DomainInfo.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/SignatureSet.h"
#include "comma/codegen/DomainInfo.h"

#include "llvm/Support/Casting.h"
#include "llvm/DerivedTypes.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

const std::string DomainInfo::getDomainInfoName() const
{
    std::string name = CodeGen::getLinkPrefix(domoid);
    name.append(domoid->getString());
    name.append("__1domain_info");
    return name;
}

void DomainInfo::emitInfoName()
{
    std::string str = getDomainInfoName();
    llvm::Constant *name = CG->emitStringLiteral(domoid->getString(), true, str);
    llvm::PointerType *nameTy = llvm::PointerType::getUnqual(llvm::Type::Int8Ty);

    infoName = llvm::ConstantExpr::getPointerCast(name, nameTy);
}

void DomainInfo::emitInfoArity()
{
    unsigned arity = 0;

    if (const FunctorDecl *functor = dyn_cast<FunctorDecl>(domoid))
        arity = functor->getArity();

    infoArity = llvm::ConstantInt::get(llvm::Type::Int32Ty, arity);
}

unsigned DomainInfo::emitOffsetsForSignature(const SignatureType *sig, unsigned offset)
{
    typedef SignatureSet::const_iterator sig_iterator;
    typedef DeclRegion::ConstDeclIter decl_iterator;

    const Sigoid *sigoid = sig->getSigoid();
    const SignatureSet& SS = sigoid->getSignatureSet();

    offsets.push_back(offset);

    for (decl_iterator iter = sigoid->beginDecls();
         iter != sigoid->endDecls(); ++iter) {
        const Decl *decl = *iter;

        if (decl->isDeclaredIn(sigoid) && isa<SubroutineDecl>(decl))
            offset++;
    }

    for (sig_iterator iter = SS.beginDirect(); iter != SS.endDirect(); ++iter)
        offset = emitOffsetsForSignature(*iter, offset);

    return offset;
}

void DomainInfo::emitSignatureOffsets()
{
    typedef SignatureSet::const_iterator iterator;
    const SignatureSet& SS = domoid->getSignatureSet();
    unsigned offset = 0;

    for (iterator iter = SS.beginDirect(); iter != SS.endDirect(); ++iter)
        offset = emitOffsetsForSignature(*iter, offset);
}

void DomainInfo::emit()
{
    emitInfoName();
    emitInfoArity();
    emitSignatureOffsets();
}
