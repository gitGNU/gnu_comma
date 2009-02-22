//===-- ast/AstRewriter.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// This class implements a map from one set of nodes to another.  In some ways,
// it is like a scope where the components of an AST can be resolved with
// respect to the "bindings" established in the rewriter.  For example, the
// AstRewriter can encapsulate the mappings from the formal to actual parameters
// of a functor, or from a % node to a concrete domain type.
//
// Methods are provided to build and interrogate the set of mappings, and to
// create new nodes using a set of mappings.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTREWRITER_HDR_GUARD
#define COMMA_AST_ASTREWRITER_HDR_GUARD

#include "comma/ast/AstBase.h"
#include <map>

namespace comma {

class AstRewriter {

public:
    // Adds a rewrite rule from the source to target.  If a mapping from the
    // given source already exists, this method will unconditionally re-target
    // the rule to the given source -- it is the responsibility of the
    // programmer to ensure that established rules are not mistakenly
    // overwritten.
    void addRewrite(Type *source, Type *target) {
        rewrites[source] = target;
    }

    // Maps source to a target if a rewrite rule exists, otherwise returns
    // source.
    Type *getRewrite(Type *source) const;

    // Returns a reference to a target entry in the rewriter corresponding to
    // source.  This returned value is an lvalue and can be used as a shorthand
    // for addRewrite.
    Type *&operator [](Type *source) {
        return rewrites[source];
    }

    // Populates this rewriter with rules which map the formal argument nodes of
    // the underlying declaration to the actual arguments provided by the
    // supplied type.  In addition, a mapping from the % node of the declaration
    // to the given type is established.  The only case in which this method is
    // a no-op is when the supplied type is a PercentType.
    void installRewrites(DomainType *context);

    // Populates this rewriter with rules which map the formal argument nodes of
    // the underlying signature declaration to the actual arguments provided by
    // the type.  This method is a no-op when the supplied type is not
    // parameterized.
    void installRewrites(SignatureType *context);

    // Returns true if the given type is rewritten to a distinct node using the
    // established rules.
    bool isRewriteSource(Type *source) const {
        return getRewrite(source) != source;
    }

    // Remove all rewrite rules.
    void clear() { rewrites.clear(); }

    // Rewrites the given signature type according to the installed rules.
    SignatureType *rewrite(SignatureType *sig) const;

    // Rewrites the given domain type according to the installed rules.
    DomainType *rewrite(DomainType *dom) const;

    // Rewrites the given subroutine type according to the installed rules.
    //
    // FIXME: Currently, a freshly `newed' node is unconditionally created, even
    // in the case where no rewrites were applicable.  This behaviour will
    // change once the allocation requirements of function type nodes are nailed
    // down.
    SubroutineType *rewrite(SubroutineType *srType) const;

    // Rewrites the given function type according to the installed rules.
    FunctionType *rewrite(FunctionType *ftype) const;

    // Rewrites the given procedure type according to the installed rules.
    ProcedureType *rewrite(ProcedureType *ftype) const;

private:
    typedef std::map<Type*, Type*> RewriteMap;
    RewriteMap rewrites;

    // Rewrites "count" parameter types of the given subroutine, placing the
    // results of the rewrite in "params".
    void rewriteParameters(SubroutineType *srType,
                           unsigned        count,
                           Type          **params) const;
};

} // End comma namespace

#endif
