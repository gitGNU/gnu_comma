//===-- ast/SignatureSet.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstRewriter.h"
#include "comma/ast/Decl.h"
#include "comma/ast/SignatureSet.h"
#include "comma/ast/Type.h"

using namespace comma;

bool SignatureSet::addDirectSignature(SignatureType *signature)
{
    if (directSignatures.insert(signature)) {
        Sigoid     *sigDecl = signature->getSigoid();
        AstRewriter rewriter;

        // Rewrite the percent node of the signature to that of the model
        // associated with this set.
        rewriter.addRewrite(sigDecl->getPercent(),
                            associatedModel->getPercent());

        // If the supplied signature is parameterized, install rewrites mapping
        // the formal parameters of the signature to the actual parameters of
        // the type.
        rewriter.installRewrites(signature);

        const SignatureSet& sigset = sigDecl->getSignatureSet();

        for (iterator iter = sigset.begin(); iter != sigset.end(); ++iter) {
            SignatureType *rewrite = rewriter.rewrite(*iter);
            allSignatures.insert(rewrite);
        }
        allSignatures.insert(signature);

        return true;
    }
    return false;
}

