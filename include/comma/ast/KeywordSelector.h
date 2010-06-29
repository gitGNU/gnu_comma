//===-- ast/KeywordSelector.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file defines the KeywordSelector class.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_KEYWORDSELECTOR_HDR_GUARD
#define COMMA_AST_KEYWORDSELECTOR_HDR_GUARD

#include "comma/ast/Expr.h"
#include "comma/ast/TypeRef.h"

namespace comma {

/// This class represents keyword selectors as used when forming (for example)
/// subroutine calls or generic instantiations.
///
/// Since the right hand side of a keyword selector can be an expression or a
/// type, depending on the context it appears in, we define two corresponding
/// kinds of selectors.  isTypeSelector returns true if its right-hand-side
/// names a type.  isExprSelector returns true if its right-hand-side denotes an
/// expression.  Several accessors are provided to retrieve the RHS as the
/// needed type.
///
/// This class is a miscellaneous member of the AST hierarchy, as it does not
/// belong to any one of its main branches (Type, Decl, Expr).  This is a
/// transient node used by the type checker to encapsulate a keyword selection,
/// and does not appear in the final AST.
class KeywordSelector : public Ast {

public:
    /// Construct a keyword selection which targets an expression.
    ///
    /// \param key  The argument keyword to be selected.
    ///
    /// \param loc  The location of \p key.
    ///
    /// \param expr The expression associated with \p key.
    KeywordSelector(IdentifierInfo *key, Location loc, Expr *expr)
        : Ast(AST_KeywordSelector),
          keyword(key), loc(loc), rhs(expr) { }

    /// Construct a keyword selection which targets a type.
    ///
    /// \param key The argument keywords to be selected.
    ///
    /// \param loc The location of \p key.
    ///
    /// \param type A TypeRef node encapsulating the type.
    KeywordSelector(IdentifierInfo *key, Location loc, TypeRef *tyRef)
        : Ast(AST_KeywordSelector),
          keyword(key), loc(loc), rhs(tyRef) { }

    /// Returns the identifier info representing the keyword.
    IdentifierInfo *getKeyword() const { return keyword; }

    /// Returns the location of the keyword for this selection.
    Location getLocation()  const { return loc; }

    /// Returns true if this selector has a type as its RHS.
    bool isTypeSelector() const { return llvm::isa<TypeRef>(rhs); }

    /// Returns true if this selector has an expression as its RHS.
    bool isExprSelector() const { return llvm::isa<Expr>(rhs); }

    //@{
    /// Sets the right hand side of this selector, overwriting the previous
    /// value.
    void setRHS(Expr *expr) { rhs = expr; }
    void setRHS(TypeRef *ref) { rhs = ref; }
    //@}

    //@{
    /// If this selector has an expression on its right hand side, returns the
    /// associated expression, otherwise null.
    Expr *getExpression() { return llvm::dyn_cast<Expr>(rhs); }
    const Expr *getExpression() const { return llvm::dyn_cast<Expr>(rhs); }
    //@}

    /// If this selector has an expression on its right hand side, and the
    /// expression is unambiguous, return the type of the associated expression
    /// and null otherwise.
    Type *getType() const {
        if (const Expr *expr = getExpression()) {
            if (expr->hasType())
                return expr->getType();
        }
        return 0;
    }

    //@{
    /// If this selector has a type on its right hand side, returns the
    /// associated TypeRef, otherwise null.
    TypeRef *getTypeRef() { return llvm::dyn_cast<TypeRef>(rhs); }
    const TypeRef *getTypeRef() const { return llvm::dyn_cast<TypeRef>(rhs); }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const KeywordSelector *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_KeywordSelector;
    }

private:
    IdentifierInfo *keyword;
    Location loc;
    Ast *rhs;
};

} // end comma namespace.

#endif
