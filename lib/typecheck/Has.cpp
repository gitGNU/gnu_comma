//===-- typecheck/Has.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Type.h"
#include "comma/ast/Decl.h"
#include <map>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

bool percentHas(ModelDecl *source, SigInstanceDecl *target)
{
    if (Sigoid *sigoid = dyn_cast<Sigoid>(source)) {
        // This % node corresponds to a signature declaration.  If the
        // declaration is not parameterized, check if the target matches the
        // source declaration.  That is to say, in the context of some signature
        // S, % certainly has S.
        if (SignatureDecl *signature = dyn_cast<SignatureDecl>(sigoid)) {
            if (signature == target->getSignature())
                return true;
        }
        else {
            // When % is defined in the context of some variety V (X1 : T1, ..,
            // Xn : Tn), check if the target corresponds to V(X1, .., Xn).
            VarietyDecl *variety = cast<VarietyDecl>(sigoid);
            if (variety == target->getVariety()) {
                bool matchFound = true;
                unsigned arity = variety->getArity();
                for (unsigned i = 0; i < arity; ++i) {
                    Type       *actual = target->getActualParameter(i);
                    DomainType *formal = variety->getFormalType(i);
                    if (actual != formal) {
                        matchFound = false;
                        break;
                    }
                }
                if (matchFound) return true;
            }
        }
        // Otherwise, an exact match on the target is sought against the set of
        // super signatures.  No rewrites are needed since this test is wrt the
        // internal view of the signature.
        return sigoid->getSignatureSet().contains(target);
    }

    // We do not have a signature, so we must have a domain.  Since we are
    // asking if % has the given signature, we are working with the 'internal
    // view' of the domain and the super signature set does not require a
    // rewrite.
    Domoid *domoid = cast<Domoid>(source);
    return domoid->getSignatureSet().contains(target);
}

} // End anonymous namespace.

bool TypeCheck::has(DomainType *source, SigInstanceDecl *target)
{
    if (PercentDecl *percent = source->getPercentDecl()) {
        return percentHas(percent->getDefinition(), target);
    }

    DomainTypeDecl *dom = source->getDomainTypeDecl();
    const SignatureSet &sigset = dom->getSignatureSet();
    SignatureSet::const_iterator iter = sigset.begin();
    SignatureSet::const_iterator endIter = sigset.end();

    for ( ; iter != endIter; ++iter) {
        SigInstanceDecl *candidate = *iter;
        if (candidate == target)
            return true;
    }
    return false;
}
