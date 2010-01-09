//===-- DeclRewriter.h ---------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the DeclRewriter class.
///
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECLREWRITER_HDR_GUARD
#define COMMA_AST_DECLREWRITER_HDR_GUARD

#include "comma/ast/AstRewriter.h"

#include "llvm/ADT/DenseMap.h"

namespace comma {

class DeclRewriter : public AstRewriter {

public:

    /// Constructs a DeclRewriter using the given AstResource to generate new
    /// AST nodes, and the given DeclRegion as a defining context for any new
    /// declarations produced.  The resulting rewriter does not contain any
    /// rewrite rules.
    DeclRewriter(AstResource &resource, DeclRegion *context)
        : AstRewriter(resource), context(context) { }

    /// Constructs a DeclRewriter using an existing AstRewriter.
    DeclRewriter(const AstRewriter &rewrites, DeclRegion *context)
        : AstRewriter(rewrites), context(context) { }

    /// \name Declaration rewrite methods.
    ///
    /// The following method rewrite declaration nodes.  A new declaration node
    /// is always returned regardless of whether any rewrite rules applied.  The
    /// declarative region to which the new node belongs is the one supplied to
    /// this rewritors ctor.
    //@{
    TypeDecl *rewriteTypeDecl(TypeDecl *tdecl);

    FunctionDecl *rewriteFunctionDecl(FunctionDecl *fdecl);

    ProcedureDecl *rewriteProcedureDecl(ProcedureDecl *pdecl);

    EnumerationDecl *rewriteEnumerationDecl(EnumerationDecl *edecl);

    ArrayDecl *rewriteArrayDecl(ArrayDecl *adecl);

    IntegerDecl *rewriteIntegerDecl(IntegerDecl *idecl);

    RecordDecl *rewriteRecordDecl(RecordDecl *rdecl);

    IncompleteTypeDecl *rewriteIncompleteTypeDecl(IncompleteTypeDecl *ITD);
    //@}

    /// Rewrites the given declaration node.
    Decl *rewriteDecl(Decl *decl);

private:
    DeclRegion *context;

    /// The table used to implement declaration rewrites.  The number of such
    /// rewrites can become quite large, so a DenseMap is appropriate here.
    typedef llvm::DenseMap<Decl*, Decl*> DeclMap;
    DeclMap declRewrites;

    /// \brief Returns a rewriten declaration node for \p source if one exists,
    /// else null.
    Decl *findRewrite(Decl *source) const { return declRewrites.lookup(source); }

    /// Returns true if the given declaration has an associated rewrite rule.
    bool hasRewrite(Decl *source) const { return findRewrite(source) != 0; }

    /// \brief Adds a declaration rewrite rule from \p source to \p target.
    /// This method will assert if a rule already exists for \p source.
    void addDeclRewrite(Decl *source, Decl *target) {
        assert(!hasRewrite(source) && "Cannot override decl rewrite rules!");
        declRewrites[source] = target;
    }

    /// Populates the rewrite map with all declarations is \p source to the
    /// corresponding declarations in \p target.
    void mirrorRegion(DeclRegion *source, DeclRegion *target);

    Expr *rewriteExpr(Expr *expr);

    IntegerLiteral *rewriteIntegerLiteral(IntegerLiteral *lit);
    FunctionCallExpr *rewriteFunctionCall(FunctionCallExpr *call);
    AttribExpr *rewriteAttrib(AttribExpr *attrib);
    ConversionExpr *rewriteConversion(ConversionExpr *conv);
};

} // end comma namespace.

#endif
