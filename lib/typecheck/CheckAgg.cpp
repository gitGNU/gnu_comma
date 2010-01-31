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
#include "comma/ast/AggExpr.h"
#include "comma/ast/AttribDecl.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/DiagPrint.h"

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

//===----------------------------------------------------------------------===//
// AggCheckerBase
//
/// \class
///
/// \brief Provides services shared between the specialized aggregate checking
/// classes.
class AggCheckerBase {

protected:
    AggCheckerBase(TypeCheck &TC) : TC(TC) { }

    TypeCheck &TC;              // TypeCheck context.

    /// Posts the given diagnostic message.
    DiagnosticStream &report(Location loc, diag::Kind kind);

    /// Returns the SourceLocation corresponding to the given Location.
    SourceLocation getSourceLoc(Location loc);
};

//===----------------------------------------------------------------------===//
// ArrayAggChecker
//
/// \class
///
/// This class provides the main typecheck logic needed to analyze aggregate
/// expressions of array type.
class ArrayAggChecker : private AggCheckerBase {

public:
    ArrayAggChecker(TypeCheck &TC)
        : AggCheckerBase(TC), refinedIndexType(0) { }

    /// Attempts to resolve an aggregate expression with respect to the given
    /// array type.  Returns an expression node (possibly different from \p agg)
    /// on success.  Otherwise, null is returned and diagnostics are posted.
    Expr *resolveAggregateExpr(AggregateExpr *agg, ArrayType *context);

private:
    /// If an array aggregate expression's index type is constrained by the
    /// aggregate, this member is filled in with the corresponding subtype.
    DiscreteType *refinedIndexType;

    /// When processing aggregates with keyed components the following vector is
    /// populated with each ComponentKey in the aggregate.
    ///
    /// \see ensureStaticKeys();
    std::vector<ComponentKey*> keyVec;

    /// Verifies that the given aggregate expression is properly structured for
    /// use as an array agggregate.
    bool ensureAggregateStructure(AggregateExpr *agg);

    /// Type checks any simple identifiers present in the given aggregates keys
    /// as direct names and replaces the identifiers with the resolved node.
    bool convertAggregateIdentifiers(AggregateExpr *agg);

    /// Helper for resolveDefiniteAggExpr.  Deals with purely positional
    /// aggregates of array type.
    Expr *resolvePositionalAggExpr(AggregateExpr *agg, ArrayType *context);

    /// Helper for resolveDefiniteAggExpr.  Deals with purely keyed aggregates
    /// of array type.
    Expr *resolveKeyedAggExpr(AggregateExpr *agg, ArrayType *context);

    /// \brief Typechecks and resolves a list of component keys appearing in an
    /// array aggregate expression.
    ///
    /// \param KL ComponentKeyList to check and resolve.
    ///
    /// \param indexTy The type which each index must resolve to.
    ///
    /// \param componentTy The type which the expression associated with \p KL
    /// must satisfy.
    ///
    /// \return True if \p KL was succesfully checked and resolved.  False
    /// otherwise.
    bool checkArrayComponentKeys(ComponentKeyList *KL, DiscreteType *indexTy,
                                 Type *componentTy);

    /// \brief Helper for resolveKeyedAggExpr.
    ///
    /// Checks a keyed array aggregate which consists of a single key and does
    /// not contain an "others" clause.
    bool checkSinglyKeyedAgg(AggregateExpr *agg, ArrayType *contextTy);

    /// \brief Helper for resolveKeyedAggExpr.
    ///
    /// Checks a keyed array aggregate which consists of multiple keys and/or
    /// contains an "others" clause.
    bool checkMultiplyKeyedAgg(AggregateExpr *agg, ArrayType *contextTy);

    /// Scans each key provided by the given aggregate.  Ensures that each key
    /// is static and non-null if bounded.  Each validated key is pushed (in
    /// order) onto keyVec.  Returns true if all keys were validated.
    bool ensureStaticKeys(AggregateExpr *agg);

    /// \brief Ensures that the current set of keys do not overlap.
    ///
    /// This method is used when checking array aggregates.
    ///
    /// Given that keyVec has been populated with static and non-null keys, this
    /// method ensures that there are no overlaps and, when \p hasOthers is
    /// false, that the keys define a continuous index set.
    ///
    /// \param contextTy The type context for this aggegate.
    ///
    /// \param hasOthers True if the aggregate under construction has an others
    /// clause.
    ///
    /// \return True if the check was successful.
    bool ensureDistinctKeys(ArrayType *contextTy, bool hasOthers);

    /// \brief Helper method for ensureDistinctKeys.
    ///
    /// Checks that the sorted ComponentKey's \p X and \p Y do not overlap and,
    /// if \p requireContinuity is true, that \p X and \p Y represent a
    /// continuous range.  The comparisons are performed using a signed or
    /// unsigned interpretation of the component key depending on the setting of
    /// \p isSigned.
    bool ensureDistinctKeys(ComponentKey *X, ComponentKey *Y,
                            bool requireContinuity, bool isSigned);

    /// \brief Checks the \c others component (if any) provided by \p agg.
    ///
    /// \return True if the \c others component is well formed with respect to
    /// \p context, or if \p agg does not admit an \c others clause.  False
    /// otherwise.
    bool checkOthers(AggregateExpr *agg, ArrayType *context);
};

//===----------------------------------------------------------------------===//
// RecordAggChecker
//
/// \class
///
/// \brief Implements type check logic for aggregate expressions of record type.
class RecordAggChecker : private AggCheckerBase {

public:
    RecordAggChecker(TypeCheck &TC)
        : AggCheckerBase(TC), ContextTy(0) { }

    /// Attempts to resolve an aggregate expression with respect to the given
    /// record type.  Returns an expression node (possibly different from \p
    /// agg) on success.  Otherwise, null is returned and diagnostics are
    /// posted.
    Expr *resolveAggregateExpr(AggregateExpr *agg, RecordType *context);

private:
    RecordType *ContextTy;      // Expected aggregate type.
    RecordDecl *ContextDecl;    // Defining declaration of ContextTy.

    // All components provided by the aggregate.
    llvm::SetVector<ComponentDecl*> ComponentVec;

    /// \brief Ensures that the given aggregate contains the right number of
    /// components for the context types.
    bool checkAggregateSize(AggregateExpr *agg);

    /// \brief Checks that the given aggregate is structurally usable as a
    /// record aggregate.
    ///
    /// This method ensures that each keyed component of the record consists
    /// only of Identifiers, and diagnoses occurrences of expressions, subtype
    /// indications, and ranges.
    ///
    /// \return True on success and false otherwise.
    bool ensureAggregateStructure(AggregateExpr *agg);

    /// \brief Checks all of the positional components in the given aggregate.
    ///
    /// Also populates ComponentVec with the ComponentDecl's satisfied by a
    /// positional component.
    bool checkPositionalComponents(AggregateExpr *agg);

    /// \brief Checks all of the keyed components in the given aggregate.
    bool checkKeyedComponents(AggregateExpr *agg);

    /// \brief Checks any `others' commponent asociated with the aggregate.
    bool checkOthersComponents(AggregateExpr *agg);

    /// \brief Helper for checkKeyedComponents.
    ///
    /// Checks that each key names an actual component, that the keys do not
    /// name any positional components, and that all key's are unique.  Also
    /// populates ComponentVec with the keyed ComponentDecl's.
    bool checkKeyConsistency(AggregateExpr *agg);

    /// \brief Helper for checkKeyedComponents.
    ///
    /// Ensures that all the keyed components the associated expressions are
    /// well typed.
    bool checkKeyTypes(AggregateExpr *expr);
};

/// helper function for acceptStringLiteral().  Extracts the enumeration
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

//===----------------------------------------------------------------------===//
// AggCheckerBase methods.

DiagnosticStream &AggCheckerBase::report(Location loc, diag::Kind kind)
{
    return TC.getDiagnostic().report(getSourceLoc(loc), kind);
}

SourceLocation AggCheckerBase::getSourceLoc(Location loc)
{
    return TC.getAstResource().getTextProvider().getSourceLocation(loc);
}

//===----------------------------------------------------------------------===//
// ArrayAggChecker methods.

bool ArrayAggChecker::ensureAggregateStructure(AggregateExpr *agg)
{
    // Empty aggregates should never persist this far.
    assert(!agg->empty() && "Unexpected empty aggregate expression!");

    // Ensure that the aggregate is purely keyed or positional.  Only record
    // aggregates can contain a mix of the two.
    if (agg->isPurelyPositional() || agg->isPurelyKeyed())
        return true;

    report(agg->getLocation(), diag::MIXED_ARRAY_AGGREGATE);
    return false;
}

bool ArrayAggChecker::convertAggregateIdentifiers(AggregateExpr *agg)
{
    // Traverse the set of keys provided by this aggregate and check any
    // identifiers as simple direct names.
    bool allOK = true;
    typedef AggregateExpr::key_iterator iterator;
    iterator I = agg->key_begin();
    iterator E = agg->key_end();
    for ( ; I != E; ++I) {
        ComponentKey *key = *I;
        if (!key->denotesIdentifier())
            continue;

        Identifier *id = key->getAsIdentifier();
        Ast *rep = TC.checkDirectName(id->getIdInfo(), id->getLocation(),
                                      /*forStatement*/ false);
        if (!rep) {
            allOK = false;
            continue;
        }

        // FIXME: The following code essentially duplicates the logic in
        // TypeCheck::acceptAggregateKey.  This code should be factored out and
        // shared.
        if (TypeRef *ref = dyn_cast<TypeRef>(rep)) {
            TypeDecl *decl = ref->getTypeDecl();
            if (!decl || !decl->getType()->isDiscreteType()) {
                report(ref->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
                allOK = false;
            }
            else
                key->setKey(ref);
        }
        else if (Expr *expr = TC.ensureExpr(rep))
            key->setKey(expr);
        else
            allOK = false;
    }
    return allOK;
}

Expr *ArrayAggChecker::resolveAggregateExpr(AggregateExpr *agg,
                                            ArrayType *context)
{
    // If the given aggregate already has a resolved type, ensure the given
    // context is compatible.
    if (agg->hasResolvedType()) {
        if (!TC.covers(agg->getType(), context)) {
            report(agg->getLocation(), diag::INCOMPATIBLE_TYPES);
            return 0;
        }
        else
            return agg;
    }

    // Ensure that the aggregate is of a compatible form.
    if (!ensureAggregateStructure(agg))
        return 0;

    // Type check any simple identifiers as direct names (now that we know these
    // identifiers do not represent record component selectors).
    if (!convertAggregateIdentifiers(agg))
        return 0;

    // FIXME: The following code does not yet support multidimensional
    // aggregates.
    assert(context->isVector() && "Multidimensional arrays not supported yet!");

    if (agg->isPurelyPositional())
        return resolvePositionalAggExpr(agg, context);
    if (agg->isPurelyKeyed())
        return resolveKeyedAggExpr(agg, context);
    assert(false && "Invalid aggregate composition!");
    return 0;
}

Expr *ArrayAggChecker::resolvePositionalAggExpr(AggregateExpr *agg,
                                                ArrayType *context)
{
    assert(agg->isPurelyPositional());

    Type *componentType = context->getComponentType();

    // Check each component of the aggregate with respect to the component type
    // of the context.
    typedef AggregateExpr::pos_iterator iterator;
    iterator I = agg->pos_begin();
    iterator E = agg->pos_end();
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

    // Otherwise, the context is unconstrained.  Generate a constrained subtype.
    // We need a constraint of the form S'First .. S'Val(L - 1), where S is the
    // index type and L is the length of this aggregate.
    DiscreteType *idxTy = context->getIndexType(0);
    ValAD *attrib = idxTy->getValAttribute();

    // Build an integer literal corresponding to the length of this aggregate.
    // Note that integer literals are represented as "minimally sized" signed
    // values.
    unsigned bits = 32 - llvm::CountLeadingZeros_32(numComponents) + 1;
    llvm::APInt L(bits, numComponents - 1);
    Expr *arg = new IntegerLiteral(L, 0);

    // Build a call to the Val attribute.
    FunctionCallExpr *upper = new FunctionCallExpr(attrib, 0, &arg, 1, 0, 0);

    // Build an attribute expression for the lower bound.
    FirstAE *lower = new FirstAE(idxTy, 0);

    // Check the lower and upper bounds in the context of the index type.
    assert(TC.checkExprInContext(lower, idxTy) && "Invalid implicit expr!");
    assert(TC.checkExprInContext(upper, idxTy) && "Invalid implicit expr!");

    // Create the discrete subtype for the index.
    DiscreteType *newIdxTy = TC.getAstResource().createDiscreteSubtype
        (idxTy, lower, upper);

    // Finally create and set the new constrained array type for the aggregate.
    ArrayType *newArrTy = TC.getAstResource().createArraySubtype
        (context->getIdInfo(), context, &newIdxTy);

    agg->setType(newArrTy);
    return agg;
}

bool ArrayAggChecker::checkSinglyKeyedAgg(AggregateExpr *agg,
                                          ArrayType *contextTy)
{
    // There is no further checking which needs to happen for an aggregate
    // containing a single keyed component and is without an others clause.
    // Simply check if the context type is unconstrained and resolve the index
    // type if needed.
    if (!contextTy->isConstrained()) {
        DiscreteType *indexTy = contextTy->getIndexType(0);
        AstResource &resource = TC.getAstResource();
        Expr *lower = (*agg->key_begin())->getLowerExpr();
        Expr *upper = (*agg->key_begin())->getUpperExpr();
        refinedIndexType = resource.createDiscreteSubtype(
            indexTy, lower, upper);
    }
    return true;
}

bool ArrayAggChecker::checkMultiplyKeyedAgg(AggregateExpr *agg,
                                            ArrayType *contextTy)
{
    bool allOK;

    // Ensure each of the keys provided by this aggregate are static and
    // non-null if bounded.  Populate keyVec with all of the valid keys.
    allOK = ensureStaticKeys(agg);

    // Check keyVec for any overlaps.
    allOK = allOK && ensureDistinctKeys(contextTy, agg->hasOthers());

    // Check the others component if present.
    return checkOthers(agg, contextTy) && allOK;
}

Expr *ArrayAggChecker::resolveKeyedAggExpr(AggregateExpr *agg,
                                           ArrayType *context)
{
    Type *componentTy = context->getComponentType();
    DiscreteType *indexTy = context->getIndexType(0);

    // Check and resolve each key list.
    bool allOK = true;
    for (AggregateExpr::kl_iterator I = agg->kl_begin(), E = agg->kl_end();
         I != E; ++I)
        allOK = checkArrayComponentKeys(*I, indexTy, componentTy) && allOK;
    if (!allOK) return 0;

    // Compute the total number of keys provided by this aggregate.
    unsigned numKeys = agg->numKeys();

    // If there is only one key and there is no others clause the aggregate is
    // permitted to be dynamic or null.  If the context type of the aggregate is
    // unconstrained then generate a new constrained subtype for the index.
    if (numKeys == 1 && !agg->hasOthers())
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

bool ArrayAggChecker::checkArrayComponentKeys(ComponentKeyList *KL,
                                              DiscreteType *indexTy,
                                              Type *componentTy)
{
    typedef ComponentKeyList::iterator iterator;
    RangeChecker rangeCheck(TC);

    // FIXME:  Currently, only ranges are permitted in ComponentKeyList's.
    for (unsigned i = 0; i < KL->numKeys(); ++i) {
        if (Range *range = KL->resolveKey<Range>(i)) {
            if (!rangeCheck.resolveRange(range, indexTy))
                return false;
        }
        else if (Expr *expr = KL->resolveKey<Expr>(i)) {
            if ((expr = TC.checkExprInContext(expr, indexTy)))
                KL->setKey(i, expr);
            else
                return false;
        }
        else {
            assert(false && "Key type not yet supported!");
            return false;
        }
    }

    // Ensure that the associated expression satisfies the component type.
    if (Expr *expr = TC.checkExprInContext(KL->getExpr(), componentTy)) {
        KL->setExpr(expr);
        return true;
    }
    else
        return false;
}

bool ArrayAggChecker::ensureStaticKeys(AggregateExpr *agg)
{
    // Iterate over the set of keys, each of which must be static and non-null
    // if bounded.  Accumulate each valid key into keyVec.
    typedef AggregateExpr::key_iterator iterator;
    iterator I = agg->key_begin();
    iterator E = agg->key_end();
    for ( ; I != E; ++I) {
        ComponentKey *key = *I;
        Location loc = key->getLocation();
        if (Range *range = key->getAsRange()) {
            if (!range->isStatic()) {
                report(loc, diag::DYNAMIC_CHOICE_NOT_UNIQUE);
                return false;
            }
            else if (range->isNull()) {
                report(loc, diag::NULL_CHOICE_NOT_UNIQUE);
                return false;
            }
        }
        else if (Expr *expr = key->getAsExpr()) {
            if (!expr->isStaticDiscreteExpr()) {
                report(loc, diag::NON_STATIC_EXPRESSION);
                return false;
            }
        }
        else {
            assert(false && "Key not yet supported!");
            return false;
        }
        keyVec.push_back(key);
    }
    return true;
}

bool ArrayAggChecker::ensureDistinctKeys(ComponentKey *X, ComponentKey *Y,
                                         bool requireContinuity, bool isSigned)
{
    llvm::APInt xValue;
    llvm::APInt yValue;
    X->getUpperValue(xValue);
    Y->getLowerValue(yValue);

    bool overlapping;
    bool continuous;
    if (isSigned) {
        int64_t x = xValue.getSExtValue();
        int64_t y = yValue.getSExtValue();
        overlapping = y <= x;
        continuous = x == y - 1;
    }
    else {
        uint64_t x = xValue.getZExtValue();
        uint64_t y = yValue.getZExtValue();
        overlapping = y <= x;
        continuous = x == y - 1;
    }

    // Diagnose overlapping indices.
    if (overlapping) {
        report(X->getLocation(), diag::DUPLICATED_AGGREGATE_COMPONENT)
            << getSourceLoc(Y->getLocation());
        return false;
    }

    // Diagnose non-continuous indices when required.
    if (requireContinuity && !continuous) {
        report(X->getLocation(), diag::DISCONTINUOUS_CHOICE)
            << getSourceLoc(Y->getLocation());
        return false;
    }

    return true;
}

bool ArrayAggChecker::ensureDistinctKeys(ArrayType *contextTy, bool hasOthers)
{
    DiscreteType *indexTy = contextTy->getIndexType(0);
    bool isSigned = indexTy->isSigned();
    bool requireContinuity = !hasOthers;

    if (isSigned)
        std::sort(keyVec.begin(), keyVec.end(), ComponentKey::compareKeysS);
    else
        std::sort(keyVec.begin(), keyVec.end(), ComponentKey::compareKeysU);

    std::vector<ComponentKey*>::iterator I = keyVec.begin();
    std::vector<ComponentKey*>::iterator E = keyVec.end();

    if (I == E)
        return true;

    ComponentKey *prev = *I;
    while (++I != E) {
        ComponentKey *next = *I;
        if (!ensureDistinctKeys(prev, next, requireContinuity, isSigned))
            return false;
        prev = next;
    }

    // If the context type of the aggregate is unconstrained then generate a new
    // constrained subtype for the current index.
    //
    // FIXME: The lower and upper bound expressions should be cloned here since
    // the new subtype will take ownership.
    if (!contextTy->isConstrained()) {
        AstResource &resource = TC.getAstResource();
        Expr *lower = keyVec.front()->getLowerExpr();
        Expr *upper = keyVec.back()->getUpperExpr();
        refinedIndexType = resource.createDiscreteSubtype(
            indexTy, lower, upper);
    }
    return true;
}

bool ArrayAggChecker::checkOthers(AggregateExpr *agg, ArrayType *context)
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

//===----------------------------------------------------------------------===//
// RecordAggChecker methods.

Expr *RecordAggChecker::resolveAggregateExpr(AggregateExpr *agg,
                                             RecordType *context)
{
    ContextTy = context;
    ContextDecl = context->getDefiningDecl();

    // If the given aggregate already has a resolved type, ensure the given
    // context is compatible.
    if (agg->hasResolvedType()) {
        if (!TC.covers(agg->getType(), context)) {
            report(agg->getLocation(), diag::INCOMPATIBLE_TYPES);
            return 0;
        }
        else
            return agg;
    }

    // Ensure that the aggregate is of a compatible form.
    if (!ensureAggregateStructure(agg))
        return 0;

    // Check that the size of the aggrgregate is sane.
    if (!checkAggregateSize(agg))
        return 0;

    // Ensure all positional components check out.
    if (!checkPositionalComponents(agg))
        return 0;

    // Ensure all keyed components check out.
    if (!checkKeyedComponents(agg))
        return 0;

    // Ensure any others clause is well formd.
    if (!checkOthersComponents(agg))
        return 0;

    agg->setType(context);
    return agg;
}

bool RecordAggChecker::ensureAggregateStructure(AggregateExpr *agg)
{
    // Iterate over any keyed components of the aggregate and ensure each
    // denotes an Identifier.
    typedef AggregateExpr::key_iterator iterator;
    iterator I = agg->key_begin();
    iterator E = agg->key_end();
    for ( ; I != E; ++I) {
        ComponentKey *key = *I;
        if (!key->denotesIdentifier()) {
            report(key->getLocation(), diag::INVALID_RECORD_SELECTOR)
                << diag::PrintType(ContextTy);
            return false;
        }
    }
    return true;
}

bool RecordAggChecker::checkAggregateSize(AggregateExpr *agg)
{
    // Ensure that the total number of keys plus the number of positional
    // components is not greater than the number of components supplied by the
    // context type.  Similarly, if there is no others clause, ensure every
    // component will be accounted for by the aggregate.
    unsigned numPositional = agg->numPositionalComponents();
    unsigned numKeys = agg->numKeys();
    unsigned numComponents = numPositional + numKeys;
    if (numComponents > ContextTy->numComponents()) {
        report(agg->getLocation(), diag::TOO_MANY_ELEMENTS_FOR_TYPE)
            << diag::PrintType(ContextTy);
        return false;
    }
    if (!agg->hasOthers() && numComponents < ContextTy->numComponents()) {
        report(agg->getLocation(), diag::TOO_FEW_ELEMENTS_FOR_TYPE)
            << diag::PrintType(ContextTy);
        return false;
    }
    return true;
}

bool RecordAggChecker::checkPositionalComponents(AggregateExpr *agg)
{
    // Ensure that the number of positional components is not greater than the
    // number of components supplied by the context type.
    if (agg->numPositionalComponents() > ContextTy->numComponents()) {
        report(agg->getLocation(), diag::TOO_MANY_ELEMENTS_FOR_TYPE)
            << diag::PrintType(ContextTy);
        return false;
    }

    // Iterate over the set of positional components and check each with respect
    // to the corresponding expected type.  Also, populate ComponentVec with all
    // valid positional components.
    typedef AggregateExpr::pos_iterator iterator;
    iterator I = agg->pos_begin();
    iterator E = agg->pos_end();
    bool allOK = true;
    for (unsigned idx = 0; I != E; ++I, ++idx) {
        Expr *expr = *I;
        ComponentDecl *decl = ContextDecl->getComponent(idx);
        Type *expectedTy = decl->getType();
        if ((expr = TC.checkExprInContext(expr, expectedTy))) {
            *I = expr;
            ComponentVec.insert(decl);
        }
        else
            allOK = false;
    }
    return allOK;
}

bool RecordAggChecker::checkKeyConsistency(AggregateExpr *agg)
{
    // Check that each key exists, that duplicate keys are not present, and that
    // keys do not overlap with any positional components.  Also, build up
    // ComponentVec with all keyed components provided by this aggregate.
    typedef AggregateExpr::key_iterator key_iterator;
    unsigned numPositional = agg->numPositionalComponents();
    key_iterator I = agg->key_begin();
    key_iterator E = agg->key_end();
    for (unsigned idx = numPositional; I != E; ++I, ++idx) {
        ComponentKey *key = *I;
        Location loc = key->getLocation();
        Identifier *id = key->getAsIdentifier();
        IdentifierInfo *name = id->getIdInfo();
        ComponentDecl *component = ContextDecl->getComponent(name);

        // Check existence.
        if (!component) {
            report(loc, diag::INVALID_RECORD_SELECTOR)
                << diag::PrintType(ContextTy);
            return false;
        }

        // Insert the component into ComponentVec if it does not already exist.
        // Diagnose overlap and uniqueness.
        if (!ComponentVec.insert(component)) {
            if (component->getIndex() < numPositional)
                report(loc, diag::COMPONENT_COVERED_POSITIONALLY);
            else {
                // Find the previous key so that we may use its location in the
                // diagnostic.
                Location conflictLoc;
                for (key_iterator P = agg->key_begin(); P != I; ++P) {
                    ComponentKey *conflict = *P;
                    if (conflict->getAsComponent() == component)
                        conflictLoc = conflict->getLocation();
                }
                assert(conflictLoc.isValid() && "Could not resolve conflict!");
                report(loc, diag::DUPLICATED_AGGREGATE_COMPONENT)
                    << getSourceLoc(conflictLoc);
            }
            return false;
        }

        // Replace the simple identifier with the corresponding component in the
        // aggregate.
        key->setKey(component);
        delete id;
    }
    return true;
}

bool RecordAggChecker::checkKeyTypes(AggregateExpr *agg)
{
    // Ensure that each list of keys are equivalently typed.  Check that the
    // associated expression satisfies the expected type.
    typedef AggregateExpr::kl_iterator kl_iterator;
    kl_iterator I = agg->kl_begin();
    kl_iterator E = agg->kl_end();
    for ( ; I != E; ++I) {
        ComponentKeyList *KL = *I;

        // If there is more than one key check that each denotes a component of
        // the same type.
        Type *listType = KL->resolveKey<ComponentDecl>(0)->getType();
        for (unsigned i = 1; i < KL->numKeys(); ++i) {
            ComponentKey *nextKey = KL->getKey(i);
            Type *nextType = nextKey->getAsComponent()->getType();
            if (listType != nextType) {
                report(nextKey->getLocation(),
                       diag::INCONSISTENT_RECORD_COMPONENT_SELECTORS);
                return false;
            }
        }

        // Check that the associated expression for this component is
        // compatible.
        if (Expr *expr = TC.checkExprInContext(KL->getExpr(), listType))
            KL->setExpr(expr);
        else
            return false;
    }
    return true;
}

bool RecordAggChecker::checkOthersComponents(AggregateExpr *agg)
{
    // If there is no others clause we are done.
    if (!agg->hasOthers())
        return true;

    // Collect the set of needed components.
    typedef RecordDecl::DeclIter decl_iterator;
    std::vector<ComponentDecl*> neededDecls;
    decl_iterator I = ContextDecl->beginDecls();
    decl_iterator E = ContextDecl->endDecls();
    for ( ; I != E; ++I) {
        ComponentDecl *neededDecl = dyn_cast<ComponentDecl>(*I);
        if (!neededDecl)
            continue;
        if (!ComponentVec.count(neededDecl))
            neededDecls.push_back(neededDecl);
    }

    // If the set is empty we are done.
    if (neededDecls.empty())
        return true;

    // Finally, ensure the set of needed decls admits components of the same
    // type and that the given others expression (if any) is compatible.
    //
    // FIXME: If there is no others expression then we need to check that each
    // component can be initialized by default or that an explicit default
    // initializer was provided in the record declaration.
    Type *neededType = neededDecls.front()->getType();
    for (unsigned i = 1; i < neededDecls.size(); ++i) {
        ComponentDecl *neededDecl = neededDecls[i];
        if (neededDecl->getType() != neededType) {
            report(agg->getOthersLoc(),
                   diag::INCONSISTENT_RECORD_COMPONENT_SELECTORS);
            return false;
        }
    }
    if (Expr *expr = agg->getOthersExpr()) {
        if (!(expr = TC.checkExprInContext(expr, neededType)))
            return false;
        agg->setOthersExpr(expr);
    }
    return true;
}

bool RecordAggChecker::checkKeyedComponents(AggregateExpr *agg)
{
    return checkKeyConsistency(agg) && checkKeyTypes(agg);
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
    if (ArrayType *arrTy = dyn_cast<ArrayType>(context)) {
        ArrayAggChecker checker(*this);
        return checker.resolveAggregateExpr(agg, arrTy);
    }
    else if (RecordType *recTy = dyn_cast<RecordType>(context)) {
        RecordAggChecker checker(*this);
        return checker.resolveAggregateExpr(agg, recTy);
    }
    else {
        report(agg->getLocation(), diag::INVALID_CONTEXT_FOR_AGGREGATE);
        return 0;
    }
}

void TypeCheck::beginAggregate(Location loc)
{
    aggregateStack.push(new AggregateExpr(loc));
}

void TypeCheck::acceptPositionalAggregateComponent(Node nodeComponent)
{
    Expr *component = ensureExpr(nodeComponent);
    nodeComponent.release();
    aggregateStack.top()->addComponent(component);
}

Node TypeCheck::acceptAggregateKey(Node lowerNode, Node upperNode)
{
    Expr *lower = ensureExpr(lowerNode);
    Expr *upper = ensureExpr(upperNode);

    if (!(lower && upper))
        return getInvalidNode();

    // We cannot resolve the type of this range until the context type is
    // given.  Build an untyped range to hold onto the bounds.
    lowerNode.release();
    upperNode.release();
    return getNode(new ComponentKey(new Range(lower, upper)));
}

Node TypeCheck::acceptAggregateKey(IdentifierInfo *name, Location loc)
{
    // Construct a ComponentKey over an IdentifierNode.  Such keys are resolved
    // during the top-down phase.
    return getNode(new ComponentKey(new Identifier(name, loc)));
}

Node TypeCheck::acceptAggregateKey(Node keyNode)
{
    // Currently, the provided node must be either a TypeRef or an Expr.
    //
    // FIXME: In the future we will have an accurate representation for subtype
    // indications that will need to be handled here.
    if (TypeRef *ref = lift_node<TypeRef>(keyNode)) {
        // Diagnose bad type references now.  Only discrete subtypes are valid
        // keys in an (array) aggregate.  Specific checks wrt the actual index
        // type are defered until the expected type of the aggregate is known.
        TypeDecl *decl = ref->getTypeDecl();
        if (!decl || !decl->getType()->isDiscreteType()) {
            report(ref->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
            return getInvalidNode();
        }
        keyNode.release();
        return getNode(new ComponentKey(ref));
    }

    Expr *expr = ensureExpr(keyNode);
    if (!expr)
        return getInvalidNode();

    keyNode.release();
    return getNode(new ComponentKey(expr));
}

void TypeCheck::acceptKeyedAggregateComponent(NodeVector &keyNodes,
                                              Node exprNode, Location loc)
{
    // When exprNode is null the parser consumed an <> token.
    Expr *expr = 0;
    if (!exprNode.isNull()) {
        if (!(expr = ensureExpr(exprNode)))
            return;
    }

    // This callback is always envoked when the expression was parsed.  If the
    // vector of keys is empty there were to many parse/semantic errors to form
    // a valid component.  Return.
    if (keyNodes.empty())
        return;

    // Convert the key nodes to their required Ast form.
    typedef NodeCaster<ComponentKey> Caster;
    typedef llvm::mapped_iterator<NodeVector::iterator, Caster> Mapper;
    typedef llvm::SmallVector<ComponentKey*, 8> KeyVec;
    KeyVec keys(Mapper(keyNodes.begin(), Caster()),
                Mapper(keyNodes.end(), Caster()));

    // Build the needed key list and add it to the current aggregate.  If expr
    // is null allocate a DiamondExpr.
    keyNodes.release();
    exprNode.release();
    ComponentKeyList *KL;
    if (expr == 0)
        expr = new DiamondExpr(loc);
    KL = ComponentKeyList::create(&keys[0], keys.size(), expr);
    aggregateStack.top()->addComponent(KL);
}

void TypeCheck::acceptAggregateOthers(Location loc, Node nodeComponent)
{
    AggregateExpr *agg = aggregateStack.top();
    Expr *component = 0;

    if (nodeComponent.isNull())
        component = new DiamondExpr(loc);
    else
        component = ensureExpr(nodeComponent);

    nodeComponent.release();
    agg->addOthersExpr(loc, component);
}

Node TypeCheck::endAggregate()
{
    AggregateExpr *agg = aggregateStack.top();
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
