//===-- ast/AstRewriter.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the AstRewriter class.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTREWRITER_HDR_GUARD
#define COMMA_AST_ASTREWRITER_HDR_GUARD

#include "comma/ast/AstBase.h"

#include <map>

namespace comma {

/// \class AstRewriter
/// \brief Rewrites Ast nodes.
///
/// This class implements a map from one set of nodes to another.  In some ways,
/// it is like a scope where the components of an AST can be resolved with
/// respect to the "bindings" established in the rewriter.  For example, the
/// AstRewriter can encapsulate the mappings from the formal to actual
/// parameters of a functor, or from a % node to a concrete domain type.
///
/// Methods are provided to build and interrogate the set of mappings, and to
/// create new nodes using the installed set of rules.
class AstRewriter {

public:
    /// Constructs an empty rewriter with no rewrite rules.
    AstRewriter(AstResource &resource) : resource(resource) { }

    /// Adds a rewrite rule from \p source to \p target.
    ///
    /// If a mapping from the given source already exists, this method will
    /// unconditionally re-target the rule -- it is the responsibility of the
    /// programmer to ensure that established rules are not mistakenly
    /// overwritten.
    void addTypeRewrite(Type *source, Type *target) {
        rewrites[source] = target;
    }

    /// \brief Adds a set of rewrites given by iterators over std::pair<Type*,
    /// Type*>.
    template <class Iter>
    void addTypeRewrites(Iter I, Iter E) {
        for ( ; I != E; ++I)
            rewrites[I->first] = I->second;
    }

    /// \brief Generates rewrite rules for a DomainType.
    ///
    /// Populates this rewriter with rules mapping the formal argument nodes of
    /// the underlying domain declaration to the actual arguments provided by \p
    /// context.  This method is a no-op when \p context is not parameterized.
    void installRewrites(DomainType *context);

    /// \brief Generates rewrite rules for a SigInstanceDecl.
    ///
    /// Populates this rewriter with rules mapping the formal argument nodes of
    /// the underlying signature declaration to the actual arguments provided by
    /// \p context.  This method is a no-op when \p context is not
    /// parameterized.
    void installRewrites(SigInstanceDecl *context);

    /// Returns true if a rewrite rule is associated with \p source.
    bool hasRewriteRule(Type *source) const {
        return getRewrite(source) != source;
    }

    /// Remove all rewrite rules.
    void clear() { rewrites.clear(); }

    /// \name Type Rewriters.
    ///
    /// \brief Rewrites the given type using the installed rules.
    ///
    /// If no rules apply to any component of the argument type, the argument is
    /// returned unchanged.  If a rewrite rule does apply, then the a new
    /// rewritten type node is returned.
    //@{
    Type *rewriteType(Type *type) const;

    DomainType *rewriteType(DomainType *dom) const;

    SubroutineType *rewriteType(SubroutineType *srType) const;

    FunctionType *rewriteType(FunctionType *ftype) const;

    ProcedureType *rewriteType(ProcedureType *ftype) const;
    //@}

    /// Rewrites the parameterization of the given SigInstanceDecl.
    ///
    /// If no rewrite rules apply, the argument is returned unchanged.
    /// Otherwise, a uniqued SigInstanceDecl is returned representing the
    /// rewritten parameterization.
    SigInstanceDecl *rewriteSigInstance(SigInstanceDecl *sig) const;

    // Returns the AstResource used to construct rewritten nodes.
    AstResource &getAstResource() const { return resource; }

protected:
    /// Returns a rewrite if it exists, otherwise null.
    Type *findRewrite(Type *source) const;

private:
    AstResource &resource;

    typedef std::map<Type*, Type*> RewriteMap;
    RewriteMap rewrites;

    /// \brief Returns a reference to a target entry in the rewriter
    /// corresponding to source.
    ///
    /// If a mapping for \p source has not been installed the returned value is
    /// null.
    Type *&operator [](Type *source) {
        return rewrites[source];
    }

    /// \brief Maps \p source to a new target if a rewrite rule exists,
    /// otherwise returns \p source.
    Type *getRewrite(Type *source) const;

    /// \brief Rewrites \p count parameter types of the given subroutine,
    /// placing the results of the rewrite in \p params.
    void rewriteParameters(SubroutineType *srType,
                           unsigned count, Type **params) const;
};

} // End comma namespace

#endif
