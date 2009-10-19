//===-- typecheck/Has.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//


#include "comma/ast/AstRewriter.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"
#include "comma/typecheck/TypeCheck.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

namespace {

bool percentHas(PercentDecl *source, SigInstanceDecl *target)
{
    if (Sigoid *sigoid = dyn_cast<Sigoid>(source->getDefinition())) {
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
                    Type *actual = target->getActualParamType(i);
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
    Domoid *domoid = cast<Domoid>(source->getDefinition());
    return domoid->getSignatureSet().contains(target);
}

} // End anonymous namespace.

bool TypeCheck::has(DomainType *source, SigInstanceDecl *target)
{
    if (PercentDecl *percent = source->getPercentDecl()) {
        return percentHas(percent, target);
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

bool TypeCheck::has(const AstRewriter &rewrites,
                    DomainType *source, AbstractDomainDecl *target)
{
    // Ensure each named signature required by the target is present in the
    // source.
    const SignatureSet &targetSigs = target->getSignatureSet();
    for (SignatureSet::const_iterator I = targetSigs.begin();
         I != targetSigs.end(); ++I) {
        SigInstanceDecl *targetSig = *I;
        if (!has(source, targetSig))
            return false;
    }

    // Traverse the set of declarations provided by the target and ensure each
    // exists in source.
    DeclRegion *sourceRegion = source->getDomainTypeDecl();
    for (DeclRegion::DeclIter I = target->beginDecls();
         I != target->endDecls(); ++I) {
        Decl *targetDecl = *I;
        IdentifierInfo *targetName = targetDecl->getIdInfo();

        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(targetDecl)) {
            // FIXME: All declarations should have an immediate bit we can
            // test.  Not just subroutines.
            if (!srDecl->isImmediate())
                continue;

            Type *targetType = rewrites.rewriteType(srDecl->getType());
            SubroutineDecl *sourceDecl = dyn_cast_or_null<SubroutineDecl>(
                sourceRegion->findDecl(targetName, targetType));
            if (!sourceDecl)
                return false;

            // Ensure the parameter models of booth the source and target decls
            // match.
            if (!sourceDecl->paramModesMatch(srDecl))
                return false;

            continue;
        }

        if (IntegerDecl *intDecl = dyn_cast<IntegerDecl>(targetDecl)) {
            // No need to rewrite, as integer types no not capture formal
            // variables.
            Type *targetType = intDecl->getType();
            if (!sourceRegion->findDecl(targetName, targetType))
                return false;
            continue;
        }

        // FIXME:
        // EnumDecls need to be compared for equivalence.
        assert(false && "Cannot handle this kind of declaration yet!");
        return false;
    }
    return true;
}
