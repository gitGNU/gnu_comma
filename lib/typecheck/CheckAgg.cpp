//===-- typecheck/CheckAgg.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Implements typecheck code related to the analysis of aggregate
/// expressions.  String literals are included here as well.
//===----------------------------------------------------------------------===//

#include "RangeChecker.h"
#include "TypeCheck.h"

#include <algorithm>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// This class provides the main typecheck logic needed to analyze aggregate
/// expressions.
class AggregateChecker {

public:
    AggregateChecker(TypeCheck &TC) :
        TC(TC), refinedIndexType(0) { }

    /// Attempts to resolve the given aggregate expression with respect to the
    /// given type.  Returns an expression node (possibly different from \p agg)
    /// on sucess.  Otherwise, null is returned and diagnostics are posted.
    Expr *resolveAggregateExpr(AggregateExpr *agg, Type *context);

private:
    TypeCheck &TC;              // TypeCheck context.

    /// If an array aggregate expression's index type is constrained by the
    /// aggregate, this member is filled in with the corresponding subtype.
    DiscreteType *refinedIndexType;

    /// When processing KeyedAggExpr's, the following vector is populated with
    /// each choice node in the aggregate.
    ///
    /// \see ensureStaticChoices();
    std::vector<Ast*> choiceVec;

    /// Posts the given diagnostic message.
    DiagnosticStream &report(Location loc, diag::Kind kind);

    /// Returns the SourceLocation corresponding to the given Location.
    SourceLocation getSourceLoc(Location loc);

    /// Helper for resolveAggregateExpr.  Deals with positional aggregates.
    Expr *resolvePositionalAggExpr(PositionalAggExpr *agg, ArrayType *context);

    /// Helper for resolveAggregateExpr.  Deals with keyed aggregates.
    Expr *resolveKeyedAggExpr(KeyedAggExpr *agg, ArrayType *context);

    /// Typechecks and resolves a choice list as provided by a KeyedAggExpr.
    ///
    /// \param CL  ChoiceList to check and resolve.
    ///
    /// \param indexTy The type which each discrete choice must resolve to.
    ///
    /// \param componentTy The type which the expression associated with \p CL
    /// must satisfy.
    ///
    /// \return True if the ChoiceList was succesfully checked and resolved.
    /// False otherwise.
    bool checkAggChoiceList(KeyedAggExpr::ChoiceList *CL,
                            DiscreteType *indexTy, Type *componentTy);

    /// Helper for resolveKeyedAggExpr.
    ///
    /// Checks a keyed aggregate which consists of a single choice and does not
    /// contain an "others" clause.
    bool checkSinglyKeyedAgg(KeyedAggExpr *agg, ArrayType *contextTy);

    /// Helper for resolveKeyedAggExpr.
    ///
    /// Checks a keyed aggregate which consists of multiple choices and/or
    /// contains an "others" clause.
    bool checkMultiplyKeyedAgg(KeyedAggExpr *agg, ArrayType *contextTy);

    /// Scans each choice provided by the given KeyedAggExpr.  Ensures that each
    /// choice is static an non-null if bounded.  Each validated choice is
    /// pushed in order onto choiceVec.  Returns true if all choices were
    /// validated.
    bool ensureStaticChoices(KeyedAggExpr *agg);

    /// Given that choiceVec has been populated with static and non-null
    /// choices, ensures that there are no overlaps and when \p hasOthers is
    /// false that the choices define a continuous index set.
    ///
    /// \param contextTy The type context for this aggegate.
    ///
    /// \param hasOthers True if the aggregate under construction has an others
    /// clause.
    ///
    /// \return True if the check was successful.
    bool ensureDistinctChoices(ArrayType *contextTy, bool hasOthers);

    /// Helper predicate for ensureDistinctChoices.  Defines a sorting between
    /// choices which have unsigned bounds or values.  For use with std::stort.
    static bool compareChoicesU(Ast *X, Ast *Y);

    /// Helper predicate for ensureDistinctChoices.  Defines a sorting between
    /// choices which have signed bounds or values.  For use with std::stort.
    static bool compareChoicesS(Ast *X, Ast *Y);

    /// Helper method for ensureDistinctChoices.
    ///
    /// Verifys that choiceVec (assumed to be in sorted order) does not contain
    /// a range of indices which lay outside any static constraints of the given
    /// index type.  If the check succeeds true is returned and Returns true if the

    /// Checks the \c others component (if any) provided by \p agg.
    ///
    /// \return True if the \c others component is well formed with respect to
    /// \p context, or if \p agg does not admit an \c others clause.  False
    /// otherwise.
    bool checkOthers(AggregateExpr *agg, ArrayType *context);

    /// \name Bounds extraction helpers.
    ///
    //@{

    /// Returns the expression representing the lower bound of the given choice
    /// node.
    static Expr *getChoiceLowerExpr(Ast *choice);

    /// Returns the expression representing the upper bound of the given choice
    /// node.
    static Expr *getChoiceUpperExpr(Ast *choice);

    /// Returns the lower bound of a static choice node. The interpretation of
    /// the result as signed or unsigned is dependent on the index type.
    static llvm::APInt getChoiceLowerBound(Ast *choice);

    /// Returns the upper bound of a static choice node. The interpretation of
    /// the result as signed or unsigned is dependent on the index type.
    static llvm::APInt getChoiceUpperBound(Ast *choice);

    /// Returns the location of the lower bound of the given choice node or the
    /// location of the node itself.
    static Location getChoiceLowerLoc(Ast *choice);

    /// Returns the location of the upper bound of the given choice node or the
    /// location of the node itself.
    static Location getChoiceUpperLoc(Ast *choice);
    //@}
};

/// Helper function for acceptStringLiteral().  Extracts the enumeration
/// declarations resulting from the lookup of a character literal.
void getVisibleEnumerations(Resolver &resolver,
                            llvm::SmallVectorImpl<EnumerationDecl*> &enums)
{
    typedef llvm::SmallVector<SubroutineDecl*, 8> RoutineBuff;
    RoutineBuff routines;

    resolver.getVisibleSubroutines(routines);

    RoutineBuff::iterator I = routines.begin();
    RoutineBuff::iterator E = routines.end();
    for ( ; I != E; ++I) {
        EnumLiteral *lit = cast<EnumLiteral>(*I);
        enums.push_back(lit->getDeclRegion());
    }
}

/// Helper function for acceptStringLiteral().  Forms the intersection of
/// component enumeration types for a string literal.
void intersectComponentTypes(StringLiteral *string,
                             llvm::SmallVectorImpl<EnumerationDecl*> &enums)
{
    typedef StringLiteral::component_iterator iterator;
    iterator I = string->begin_component_types();
    while (I != string->end_component_types()) {
        EnumerationDecl *decl = *I;
        ++I;
        if (std::find(enums.begin(), enums.end(), decl) == enums.end())
            string->removeComponentType(decl);
    }
}

} // end anonymous namespace.

DiagnosticStream &AggregateChecker::report(Location loc, diag::Kind kind)
{
    return TC.getDiagnostic().report(getSourceLoc(loc), kind);
}

SourceLocation AggregateChecker::getSourceLoc(Location loc)
{
    return TC.getAstResource().getTextProvider().getSourceLocation(loc);
}

Expr *AggregateChecker::resolveAggregateExpr(AggregateExpr *agg, Type *context)
{
    // If the type does not denote an array type we cannot resolve this
    // aggregate.
    //
    // FIXME: Support record aggregates here too.
    ArrayType *arrTy = dyn_cast<ArrayType>(context);
    if (!arrTy) {
        report(agg->getLocation(), diag::INVALID_CONTEXT_FOR_AGGREGATE);
        return 0;
    }

    // If the given arrgregate already has a resolved type, ensure the given
    // context is compatable.
    if (agg->hasType()) {
        if (!TC.covers(agg->getType(), context)) {
            report(agg->getLocation(), diag::INCOMPATIBLE_TYPES);
            return 0;
        }
        else
            return agg;
    }

    // FIXME: The following code does not yet support multidimensional
    // aggregates.
    assert(arrTy->isVector() && "Multidimensional arrays not supported yet!");

    // Otherwise, we must resolve based on the type of aggregate we were given.
    if (PositionalAggExpr *PAE = dyn_cast<PositionalAggExpr>(agg))
        return resolvePositionalAggExpr(PAE, arrTy);

    KeyedAggExpr *KAE = cast<KeyedAggExpr>(agg);
    return resolveKeyedAggExpr(KAE, arrTy);
}

Expr *AggregateChecker::resolvePositionalAggExpr(PositionalAggExpr *agg,
                                                 ArrayType *context)
{
    Type *componentType = context->getComponentType();

    // Check each component of the aggregate with respect to the component type
    // of the context.
    typedef PositionalAggExpr::iterator iterator;
    iterator I = agg->begin_components();
    iterator E = agg->end_components();
    bool allOK = true;
    for ( ; I != E; ++I) {
        if (Expr *component = TC.checkExprInContext(*I, componentType))
            *I = component;
        else
            allOK = false;
    }

    if (!allOK || !checkOthers(agg, context))
        return 0;

    unsigned numComponents = agg->numComponents();

    // If the context type is statically constrained ensure that the aggregate
    // is within the bounds of the corresponding index type.
    if (context->isStaticallyConstrained()) {
        DiscreteType *idxTy = context->getIndexType(0);
        Range *range = idxTy->getConstraint();
        uint64_t length = range->length();

        if (length < numComponents) {
            report(agg->getLocation(), diag::TOO_MANY_ELEMENTS_FOR_TYPE)
                << context->getIdInfo();
            return 0;
        }

        if (length > numComponents && !agg->hasOthers()) {
            report(agg->getLocation(), diag::TOO_FEW_ELEMENTS_FOR_TYPE)
                << context->getIdInfo();
            return 0;
        }

        // Associate the context type with this array aggregate.
        agg->setType(context);
        return agg;
    }

    // If context is dynamically constrained assign an unconstrained type to the
    // aggregate and wrap it in a conversion expression.
    if (context->isConstrained()) {
        ArrayType *unconstrainedTy;
        unconstrainedTy = TC.getAstResource().createArraySubtype(0, context);
        agg->setType(unconstrainedTy);
        return new ConversionExpr(agg, context);
    }

    // Otherwise, the context is unconstrained.
    //
    // FIXME: Generate a constrained subtype here.
    agg->setType(context);
    return agg;
}

bool AggregateChecker::checkSinglyKeyedAgg(KeyedAggExpr *agg,
                                           ArrayType *contextTy)
{
    // There is no further checking which needs to happen for an aggregate
    // containing a single choice and is without an others clause.  Simply check
    // if the context type is unconstrained and resolve the index type if
    // needed.
    if (!contextTy->isConstrained()) {
        DiscreteType *indexTy = contextTy->getIndexType(0);
        AstResource &resource = TC.getAstResource();
        Expr *lower = getChoiceLowerExpr(*agg->choice_begin());
        Expr *upper = getChoiceUpperExpr(*agg->choice_begin());
        refinedIndexType = resource.createDiscreteSubtype(
            indexTy->getIdInfo(), indexTy, lower, upper);
    }
    return true;
}

bool AggregateChecker::checkMultiplyKeyedAgg(KeyedAggExpr *agg,
                                             ArrayType *contextTy)
{
    bool allOK;

    // Ensure each of the choices provided by this aggregate are static and
    // non-null if bounded.  Populate choiceVec with the valid choices.
    allOK = ensureStaticChoices(agg);

    // Check the choiceVec for any overlapping choices.
    allOK = allOK && ensureDistinctChoices(contextTy, agg->hasOthers());

    // Check the others component if present.
    return checkOthers(agg, contextTy) && allOK;
}

Expr *AggregateChecker::resolveKeyedAggExpr(KeyedAggExpr *agg,
                                            ArrayType *context)
{
    Type *componentTy = context->getComponentType();
    DiscreteType *indexTy = context->getIndexType(0);

    // Check and resolve each discrete choice list.
    bool allOK = true;
    for (KeyedAggExpr::cl_iterator I = agg->cl_begin(); I != agg->cl_end(); ++I)
        allOK = checkAggChoiceList(*I, indexTy, componentTy) && allOK;
    if (!allOK) return 0;

    // Compute the total number of choices provided by this aggregate.
    unsigned numChoices = agg->numChoices();

    // If there is only one choice, and there is no others clause, it is
    // permitted to be dynamic or null.  If the context type of the aggregate is
    // unconstrained then generate a new constrained subtype for the index.
    if (numChoices == 1 && !agg->hasOthers())
        allOK = checkSinglyKeyedAgg(agg, context);
    else
        allOK = checkMultiplyKeyedAgg(agg, context);
    if (!allOK) return 0;

    // Build a new array subtype for the aggregate if the index types were
    // refined.
    if (refinedIndexType) {
        AstResource &resource = TC.getAstResource();
        context = resource.createArraySubtype(context->getIdInfo(), context,
                                              &refinedIndexType);
    }

    // FIXME:  Check if a conversion is required.
    agg->setType(context);
    return agg;
}

bool AggregateChecker::checkAggChoiceList(KeyedAggExpr::ChoiceList *CL,
                                          DiscreteType *indexTy,
                                          Type *componentTy)
{
    typedef KeyedAggExpr::ChoiceList::iterator iterator;
    RangeChecker rangeCheck(TC);
    bool allOK = true;

    // FIXME:  Currently, only ranges are permitted in ChoiceLists.
    for (unsigned i = 0; i < CL->numChoices(); ++i) {
        if (Range *range = CL->getChoice<Range>(i)) {
            // Check and resolve the range to the given indexTy.
            if (!rangeCheck.resolveRange(range, indexTy))
                allOK = false;
        }
        else {
            assert(false && "Discrete choice not yet supported!");
            return false;
        }
    }

    // Ensure that the associated expression satisfies the component type.
    if (Expr *expr = TC.checkExprInContext(CL->getExpr(), componentTy))
        CL->setExpr(expr);
    else
        allOK = false;

    return allOK;
}

bool AggregateChecker::ensureStaticChoices(KeyedAggExpr *agg)
{
    // Iterate over the set of choices, each of which must be static and
    // non-null if bounded.  Accumulate each valid choice into choiceVec.
    typedef KeyedAggExpr::choice_iterator iterator;
    iterator I = agg->choice_begin();
    iterator E = agg->choice_end();
    bool allOK = true;

    for ( ; I != E; ++I) {
        if (Range *range = dyn_cast<Range>(*I)) {
            if (!range->isStatic()) {
                report(range->getLowerLocation(),
                       diag::DYNAMIC_CHOICE_NOT_UNIQUE);
                allOK = false;
            }
            else if (range->isNull()) {
                report(range->getLowerLocation(),
                       diag::NULL_CHOICE_NOT_UNIQUE);
                allOK = false;
            }
            else
                choiceVec.push_back(range);
        }
        else {
            assert(false && "Discrete choice not yet supported!");
            return false;
        }
    }

    return allOK;
}

bool AggregateChecker::ensureDistinctChoices(ArrayType *contextTy,
                                             bool hasOthers)
{
    DiscreteType *indexTy = contextTy->getIndexType(0);
    bool isSigned = indexTy->isSigned();

    if (isSigned)
        std::sort(choiceVec.begin(), choiceVec.end(), compareChoicesS);
    else
        std::sort(choiceVec.begin(), choiceVec.end(), compareChoicesU);

    std::vector<Ast*>::iterator I = choiceVec.begin();
    std::vector<Ast*>::iterator E = choiceVec.end();

    if (I == E)
        return true;

    Ast *prev = *I;
    while (++I != E) {
        Ast *next = *I;

        // FIXME:  Only ranges are supported currently.
        Range *first = cast<Range>(prev);
        Range *second = cast<Range>(next);
        bool overlapping;
        bool continuous;

        if (isSigned) {
            int64_t x = first->getStaticUpperBound().getSExtValue();
            int64_t y = second->getStaticLowerBound().getSExtValue();
            overlapping = y <= x;
            continuous = x == y - 1;
        }
        else {
            uint64_t x = first->getStaticUpperBound().getZExtValue();
            uint64_t y = second->getStaticLowerBound().getZExtValue();
            overlapping = y <= x;
            continuous = x == y - 1;
        }

        // Diagnose overlapping ranges.
        if (overlapping) {
            report(first->getLowerLocation(),
                   diag::DUPLICATED_CHOICE_VALUE)
                << getSourceLoc(second->getLowerLocation());
            return false;
        }

        // Diagnose non-continuous indices when there is no others clause.
        if (!hasOthers && !continuous) {
            report(first->getUpperLocation(), diag::DISCONTINUOUS_CHOICE)
                << getSourceLoc(second->getLowerLocation());
            return false;
        }

        prev = next;
    }

    // If the context type of the aggregate is unconstrained then generate a new
    // constrained subtype for the current index.
    //
    // FIXME: The lower and upper bound expressions should be cloned here since
    // the new subtype will take ownership.
    if (!contextTy->isConstrained()) {
        AstResource &resource = TC.getAstResource();
        Expr *lower = getChoiceLowerExpr(choiceVec.front());
        Expr *upper = getChoiceUpperExpr(choiceVec.back());
        refinedIndexType = resource.createDiscreteSubtype(
            indexTy->getIdInfo(), indexTy, lower, upper);
    }
    return true;
}

bool AggregateChecker::compareChoicesU(Ast *X, Ast *Y)
{
    uint64_t boundX;
    uint64_t boundY;

    // FIXME:  Currently only ranges are supported.
    Range *range;

    range = cast<Range>(X);
    boundX = range->getStaticLowerBound().getZExtValue();

    range = cast<Range>(Y);
    boundY = range->getStaticLowerBound().getZExtValue();

    return boundX < boundY;
}

bool AggregateChecker::compareChoicesS(Ast *X, Ast *Y)
{
    int64_t boundX;
    int64_t boundY;

    // FIXME:  Currently only ranges are supported.
    Range *range;

    range = cast<Range>(X);
    boundX = range->getStaticLowerBound().getSExtValue();

    range = cast<Range>(Y);
    boundY = range->getStaticLowerBound().getSExtValue();

    return boundX < boundY;
}

bool AggregateChecker::checkOthers(AggregateExpr *agg, ArrayType *context)
{
    // Check the others component if present with respect to the component type
    // of the array.
    if (Expr *expr = agg->getOthersExpr()) {
        if (!(expr = TC.checkExprInContext(expr, context->getComponentType())))
            return false;
        else
            agg->setOthersExpr(expr);
    }

    // If the context type is unconstrained, ensure that an others component is
    // not present.
    if (!context->isConstrained() && agg->hasOthers()) {
        report(agg->getOthersLoc(), diag::OTHERS_IN_UNCONSTRAINED_CONTEXT);
        return false;
    }

    return true;
}

Expr *AggregateChecker::getChoiceLowerExpr(Ast *choice)
{
    // FIXME: Support discrete types and static expressions.
    Range *range = cast<Range>(choice);
    return range->getLowerBound();
}

Expr *AggregateChecker::getChoiceUpperExpr(Ast *choice)
{
    // FIXME: Support discrete types and static expressions.
    Range *range = cast<Range>(choice);
    return range->getUpperBound();
}

llvm::APInt AggregateChecker::getChoiceLowerBound(Ast *choice)
{
    // FIXME: Support discrete types and static expressions.
    Range *range = cast<Range>(choice);
    return range->getStaticLowerBound();
}

llvm::APInt AggregateChecker::getChoiceUpperBound(Ast *choice)
{
    // FIXME: Support discrete types and static expressions.
    Range *range = cast<Range>(choice);
    return range->getStaticUpperBound();
}

Location AggregateChecker::getChoiceLowerLoc(Ast *choice)
{
    // FIXME: Support discrete types and static expressions.
    Range *range = cast<Range>(choice);
    return range->getLowerLocation();
}

Location AggregateChecker::getChoiceUpperLoc(Ast *choice)
{
    // FIXME: Support discrete types and static expressions.
    Range *range = cast<Range>(choice);
    return range->getUpperLocation();
}

//===----------------------------------------------------------------------===//
// Public interface provided by this file.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Aggregate expression routines.

// This is the top-down pass which does the real work in checking an aggregate
// expression.
Expr *TypeCheck::resolveAggregateExpr(AggregateExpr *agg, Type *context)
{
    AggregateChecker checker(*this);
    return checker.resolveAggregateExpr(agg, context);
}

void TypeCheck::beginAggregate(Location loc, bool isPositional)
{
    if (isPositional)
        aggregateStack.push(new PositionalAggExpr(loc));
    else
        aggregateStack.push(new KeyedAggExpr(loc));
}

void TypeCheck::acceptAggregateComponent(Node nodeComponent)
{
    Expr *component = ensureExpr(nodeComponent);
    PositionalAggExpr *agg = cast<PositionalAggExpr>(aggregateStack.top());
    nodeComponent.release();
    agg->addComponent(component);
}

void TypeCheck::acceptAggregateComponent(Node lowerNode, Node upperNode,
                                         Node exprNode)
{
    // The aggregate stack should always have a KeyedAggExpr on top if the
    // parser is doing its job.
    KeyedAggExpr *agg = cast<KeyedAggExpr>(aggregateStack.top());

    Expr *lower = ensureExpr(lowerNode);
    Expr *upper = ensureExpr(upperNode);
    Expr *expr = ensureExpr(exprNode);

    if (!(lower && upper && expr))
        return;

    // We cannot resolve the type of this range until the context type is
    // given.  Build an untyped range to hold onto the bounds.
    lowerNode.release();
    upperNode.release();
    exprNode.release();
    Ast *range = new Range(lower, upper);
    agg->addDiscreteChoice(&range, 1, expr);
}

void TypeCheck::acceptAggregateOthers(Location loc, Node nodeComponent)
{
    AggregateExpr *agg = aggregateStack.top();

    if (nodeComponent.isNull())
        agg->addOthersUndef(loc);
    else {
        Expr *component = ensureExpr(nodeComponent);
        nodeComponent.release();
        agg->addOthersExpr(loc, component);
    }
}

Node TypeCheck::endAggregate()
{
    AggregateExpr *agg = cast<AggregateExpr>(aggregateStack.top());
    aggregateStack.pop();

    // It is possible that the parser could not generate a single valid
    // component, or that every parsed component did not make it thru the type
    // checker.  Deallocate.
    if (agg->empty()) {
        delete agg;
        return getInvalidNode();
    }
    return getNode(agg);
}

//===----------------------------------------------------------------------===//
// String literal routines.

Node TypeCheck::acceptStringLiteral(const char *chars, unsigned len,
                                    Location loc)
{
    // The parser provides us with a string which includes the quotation marks.
    // This means that the given length is at least 2.  Our internal
    // representation drops the outer quotes.
    assert(len >= 2 && chars[0] == '"' && chars[len - 1] == '"' &&
           "Unexpected string format!");

    const char *I = chars + 1;
    const char *E = chars + len - 1;
    StringLiteral *string = new StringLiteral(I, E, loc);

    char buff[3] = { '\'', 0, '\'' };
    typedef llvm::SmallVector<EnumerationDecl*, 8> LitVec;

    while (I != E) {
        buff[1] = *I;
        IdentifierInfo *id = resource.getIdentifierInfo(&buff[0], 3);
        Resolver &resolver = scope.getResolver();
        LitVec literals;

        ++I;
        resolver.resolve(id);
        getVisibleEnumerations(resolver, literals);

        // We should always have a set of visible subroutines.  Character
        // literals are modeled as functions with "funny" names (therefore, they
        // cannot conflict with any other kind of declaration), and the language
        // defined character types are always visible (unless hidden by a
        // character declaration of the name name).
        assert(!literals.empty() && "Failed to resolve character literal!");

        // If the string literal has zero interpretaions this must be the first
        // character in the string.  Add each resolved declaration.
        if (string->zeroComponentTypes()) {
            string->addComponentTypes(literals.begin(), literals.end());
            continue;
        }

        // Form the intersection of the current component types with the
        // resolved types.
        intersectComponentTypes(string, literals);

        // The result of the interesction should never be zero since the
        // language defined character types supply declarations for all possible
        // literals.
        assert(!string->zeroComponentTypes() && "Disjoint character types!");
    }

    return getNode(string);
}

Expr *TypeCheck::resolveStringLiteral(StringLiteral *strLit, Type *context)
{
    // First, ensure the type context is a string array type.
    ArrayType *arrTy = dyn_cast<ArrayType>(context);
    if (!arrTy || !arrTy->isStringType()) {
        report(strLit->getLocation(), diag::INCOMPATIBLE_TYPES);
        return 0;
    }

    // FIXME: Typically all contexts which involve unconstrained array types
    // resolve the context.  Perhaps we should assert that the supplied type is
    // constrained.  For now, construct an appropriate type for the literal.
    if (!arrTy->isConstrained() &&
        !(arrTy = getConstrainedArraySubtype(arrTy, strLit)))
        return 0;

    // The array is a string type.  Check that the string literal has at least
    // one interpretation of its components which matches the component type of
    // the target.
    //
    // FIXME: more work needs to be done here when the enumeration type is
    // constrained.
    EnumerationType *enumTy;
    enumTy = cast<EnumerationType>(arrTy->getComponentType());
    if (!strLit->containsComponentType(enumTy)) {
        report(strLit->getLocation(), diag::STRING_COMPONENTS_DO_NOT_SATISFY)
            << enumTy->getIdInfo();
        return 0;
    }

    // If the array type is statically constrained, ensure that the string is of
    // the proper width.  Currently, all constrained array indices are
    // statically constrained.
    uint64_t arrLength = arrTy->length();
    uint64_t strLength = strLit->length();

    if (arrLength < strLength) {
        report(strLit->getLocation(), diag::TOO_MANY_ELEMENTS_FOR_TYPE)
            << arrTy->getIdInfo();
        return 0;
    }
    if (arrLength > strLength) {
        report(strLit->getLocation(), diag::TOO_FEW_ELEMENTS_FOR_TYPE)
            << arrTy->getIdInfo();
        return 0;
    }

    /// Resolve the component type of the literal to the component type of
    /// the array and set the type of the literal to the type of the array.
    strLit->resolveComponentType(enumTy);
    strLit->setType(arrTy);
    return strLit;
}
