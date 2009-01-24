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
#include "TypeEqual.h"
#include <map>

using namespace comma;
using llvm::dyn_cast;
using llvm::isa;

namespace {

// This function is used to determine if the Sigoid `source' has a super
// signature exactly equal to target.
bool hasExactSignature(Sigoid *source, SignatureType *target)
{
    Sigoid::sig_iterator iter    = source->beginSupers();
    Sigoid::sig_iterator endIter = source->endSupers();

    for ( ; iter != endIter; ++iter)
        if (*iter == target) return true;
    return false;
}

bool abstractDomainHas(AbstractDomainDecl *source, SignatureType *target)
{
    AstRewriter    rewrites;
    SignatureType *signature = source->getSignatureType();
    Sigoid        *sigoid    = signature->getDeclaration();

    if (signature == target) return true;

    // Set up our rewrite context.  First install a mapping from the % of the
    // underlying signature declaration to that of the abstract domain.
    // Rewrites involving the signature map the formal parameters of the
    // declaration (if any) to the actual parameters of the type.  This rewrite
    // provides a public view of the signature corresponding to the given
    // abstract domain.
    rewrites.addRewrite(sigoid->getPercent(), source->getType());
    rewrites.installRewrites(signature);

    Sigoid::sig_iterator iter    = sigoid->beginSupers();
    Sigoid::sig_iterator endIter = sigoid->endSupers();
    for ( ; iter != endIter; ++iter) {
        SignatureType *candidate = *iter;
        if (compareTypesUsingRewrites(rewrites, candidate, target))
            return true;
    }
    return false;
}

bool percentHas(ModelDecl *source, SignatureType *target)
{
    if (Sigoid *sigoid = dyn_cast<Sigoid>(source)) {
        // This % node corresponds to a signature declaration.  If the
        // declaration is not parameterized, check if the target matches the
        // source declaration.  That is to say, in the context of some signature
        // S, % certainly has S.
        if (SignatureDecl *signature = dyn_cast<SignatureDecl>(sigoid)) {
            if (signature == target->getDeclaration())
                return true;
        }
        else {
            // When % is defined in the context of some variety V (X1 : T1, ..,
            // Xn : Tn), check if the target corresponds to V(X1, .., Xn).
            VarietyDecl *variety = dyn_cast<VarietyDecl>(sigoid);
            if (variety == target->getDeclaration()) {
                bool matchFound = true;
                unsigned arity = variety->getArity();
                for (unsigned i = 0; i < arity; ++i) {
                    DomainType *actual = target->getActualParameter(i);
                    DomainType *formal = variety->getFormalDomain(i);
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
        return hasExactSignature(sigoid, target);
    }

    // We do not have a signature, so we must have a domain.  Since we are
    // asking if % has the given domain, we are working with the 'internal view'
    // of the domain and the super signature set does not require a rewrite.
    Domoid *domoid = dyn_cast<Domoid>(source);
    SignatureDecl *principleSig = domoid->getPrincipleSignature();
    return hasExactSignature(principleSig, target);
}

} // End anonymous namespace.

bool TypeCheck::has(DomainType *source, SignatureType *target)
{
    if (AbstractDomainDecl *domain = source->getAbstractDecl())
        return abstractDomainHas(domain, target);
    else if (source->denotesPercent())
        return percentHas(source->getDeclaration(), target);

    Domoid *domoid = source->getDomoidDecl();
    SignatureDecl *principleSignature = domoid->getPrincipleSignature();

    SignatureDecl::sig_iterator iter = principleSignature->beginSupers();
    SignatureDecl::sig_iterator endIter = principleSignature->endSupers();
    AstRewriter rewrites;

    rewrites.installRewrites(source);
    for ( ; iter != endIter; ++iter) {
        SignatureType *candidate = *iter;
        if (compareTypesUsingRewrites(rewrites, candidate, target))
            return true;
    }
    return false;
}