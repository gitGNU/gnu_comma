//===-- DeclRewriter.h ---------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
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
    /// AST nodes.  The \p context parameter defines the declarative region in
    /// which any rewritten declarations are to be declared in.  The \p origin
    /// parameter defines the source of the declarations to be rewritten.  The
    /// resulting rewriter does not contain any rewrite rules.
    DeclRewriter(AstResource &resource,
                 DeclRegion *context, DeclRegion *origin)
        : AstRewriter(resource), context(context), origin(origin) { }

    /// Constructs a DeclRewriter using an existing AstRewriter.
    DeclRewriter(const AstRewriter &rewrites,
                 DeclRegion *context, DeclRegion *origin)
        : AstRewriter(rewrites), context(context), origin(origin) { }

    /// Switches the context and origin associated with this decl rewriter.
    ///
    /// \note It is only permited to switch a DeclRewriter to a context and
    /// origin which are both immediately enclosed by the current context and
    /// origin.
    void setContext(DeclRegion *context, DeclRegion *origin) {
        assert(context->getParent() == this->context);
        assert(origin->getParent() == this->origin);
        this->context = context;
        this->origin = origin;
    }

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
    EnumSubtypeDecl *rewriteEnumSubtypeDecl(EnumSubtypeDecl *decl);

    ArrayDecl *rewriteArrayDecl(ArrayDecl *adecl);

    IntegerDecl *rewriteIntegerDecl(IntegerDecl *idecl);
    IntegerSubtypeDecl *rewriteIntegerSubtypeDecl(IntegerSubtypeDecl *decl);

    RecordDecl *rewriteRecordDecl(RecordDecl *rdecl);

    IncompleteTypeDecl *rewriteIncompleteTypeDecl(IncompleteTypeDecl *ITD);

    AccessDecl *rewriteAccessDecl(AccessDecl *access);

    CarrierDecl *rewriteCarrierDecl(CarrierDecl *carrier);
    //@}

    /// Rewrites the given declaration node.
    Decl *rewriteDecl(Decl *decl);

private:
    DeclRegion *context;
    DeclRegion *origin;

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

    Type *rewriteType(Type *type);
    AccessType *rewriteAccessType(AccessType *type);

    Expr *rewriteExpr(Expr *expr);

    IntegerLiteral *rewriteIntegerLiteral(IntegerLiteral *lit);
    FunctionCallExpr *rewriteFunctionCall(FunctionCallExpr *call);
    AttribExpr *rewriteAttrib(AttribExpr *attrib);
    ConversionExpr *rewriteConversion(ConversionExpr *conv);
};

} // end comma namespace.

#endif
