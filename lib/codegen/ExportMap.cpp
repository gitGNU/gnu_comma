//===-- codegen/ExportMap.cpp --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "ExportMap.h"
#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"

#include <iostream>

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void ExportMap::initializeUsingSignature(SignatureEntry &entry,
                                         const Sigoid *sig)
{
    typedef SignatureSet::const_iterator sig_iterator;
    typedef DeclRegion::ConstDeclIter decl_iterator;

    unsigned index = 0;

    entry.sigoid = sig;
    entry.offsets.push_back(index);

    for (decl_iterator iter = sig->beginDecls();
         iter != sig->endDecls(); ++iter) {
        const SubroutineDecl *decl = dyn_cast<SubroutineDecl>(*iter);
        if (decl && decl->isImmediate()) {
            ExportPair pair(decl, index);
            entry.exports.push_back(pair);
            index++;
        }
    }

    // Add each direct supersignature to the gobal table and adjust the offset
    // vector.
    const SignatureSet &SS = sig->getSignatureSet();
    for (sig_iterator iter = SS.beginDirect(); iter != SS.endDirect(); ++iter) {
        const SignatureEntry superEntry = addSignature((*iter)->getSigoid());

        for (offset_iterator osi = begin_offsets(superEntry);
             osi != end_offsets(superEntry); ++osi)
            entry.offsets.push_back(*osi + index);

        index += superEntry.totalExports;
    }

    // Index holds the total number of exports for this entry.
    entry.totalExports = index;
}

unsigned ExportMap::getSignatureOffset(const SubroutineDecl *decl)
{
    typedef SignatureSet::const_iterator iterator;

    const Sigoid *ctx = resolveSignatureContext(decl);
    const Sigoid *origin = resolveSignatureOrigin(decl);

    if (ctx == origin)
        return 0;

    const SignatureSet &SS = ctx->getSignatureSet();
    unsigned offset = 0;
    for (iterator iter = SS.begin(); iter != SS.end(); ++iter) {
        offset++;
        if ((*iter)->getSigoid() == origin)
            break;
    }

    assert(offset != 0 && "Declaration origin not accessible thru context!");
    return offset;
}

unsigned ExportMap::getLocalIndex(const SubroutineDecl *decl)
{
    const SubroutineDecl *key = decl->resolveOrigin();
    const Sigoid *sig = resolveSignatureOrigin(decl);
    const SignatureEntry &entry = lookupSignature(sig);

    for (std::vector<ExportPair>::const_iterator iter = entry.exports.begin();
         iter != entry.exports.end(); ++iter) {
        if (iter->first == key)
            return iter->second;
    }

    assert(false && "Export lookup failed!");
    return ~0u;
}

unsigned ExportMap::getIndex(const SubroutineDecl *decl)
{
    unsigned sigIdx   = getSignatureOffset(decl);
    unsigned localIdx = getLocalIndex(decl);
    const Sigoid *ctx = resolveSignatureContext(decl);
    const SignatureEntry &entry = lookupSignature(ctx);

    return entry.offsets[sigIdx] + localIdx;
}

const Sigoid *ExportMap::resolveSignatureOrigin(const SubroutineDecl *decl)
{
    const SubroutineDecl *origin = decl->resolveOrigin();
    return cast<Sigoid>(origin->getDeclRegion());
}

const Sigoid *ExportMap::resolveSignatureContext(const SubroutineDecl *decl)
{
    const Sigoid *sigoid = 0;
    const DeclRegion *region = decl->getDeclRegion();

    if (const AbstractDomainDecl *add = dyn_cast<AbstractDomainDecl>(region))
        sigoid = add->getSignatureType()->getSigoid();
    else if (isa<Domoid>(region)) {
        assert(!decl->isImmediate() &&
               "Cannot resolve declarations immediate to a domain!");
        const SubroutineDecl *origin = decl->getOrigin();
        sigoid = cast<Sigoid>(origin->getDeclRegion());
    }
    else
        sigoid = cast<Sigoid>(region);

    return sigoid;
}

void ExportMap::dump(const SignatureKey entry)
{
    const Sigoid *sigoid = entry.sigoid;

    std::cerr << "SignatureEntry : " << sigoid->getString() << '\n';
    std::cerr << " Offsets :";
    for (offset_iterator iter = begin_offsets(entry);
         iter != end_offsets(entry); ++iter)
        std::cerr << ' ' << *iter;
    std::cerr << '\n';
    std::cerr << " Total # Exports : " << entry.totalExports << '\n';
    std::cerr << " Direct Exports :\n";
    for (std::vector<ExportPair>::const_iterator iter = entry.exports.begin();
         iter != entry.exports.end(); ++iter) {
        std::cerr << "   "
                  << CodeGen::getLinkName(iter->first) << " : " << iter->second
                  << '\n';
    }

    std::cerr << " All Exports :\n";
    for (Sigoid::ConstDeclIter iter = sigoid->beginDecls();
         iter != sigoid->endDecls(); ++iter) {
        const Decl *decl = *iter;
        if (const SubroutineDecl *sr = dyn_cast<SubroutineDecl>(decl)) {
            std::cerr << "   "
                      << CodeGen::getLinkName(sr) << " : " << getIndex(sr)
                      << '\n';
        }
    }
}
