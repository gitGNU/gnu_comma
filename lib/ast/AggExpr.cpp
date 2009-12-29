//===-- ast/AggExpr.cpp --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AggExpr.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// ComponentKey

bool ComponentKey::isStatic() const
{
    if (const Range *range = getAsRange())
        return range->isStatic();

    if (const Expr *expr = getAsExpr())
        return expr->isStaticDiscreteExpr();

    if (const DiscreteType *type = getAsDiscreteType()) {
        if (type->isConstrained())
            return type->getConstraint()->isStatic();
        else
            return true;
    }

    assert(denotesIdentifier() || denotesComponent());
    return true;
}

Expr *ComponentKey::getLowerExpr()
{
    if (Range *range = getAsRange())
        return range->getLowerBound();
    else
        return getAsExpr();
}

Expr *ComponentKey::getUpperExpr()
{
    if (Range *range = getAsRange())
        return range->getUpperBound();
    else
        return getAsExpr();
}

void ComponentKey::getLowerValue(llvm::APInt &value) const
{
    if (const Range *range = getAsRange())
        value = range->getStaticLowerBound();
    else
        getAsExpr()->staticDiscreteValue(value);
}

void ComponentKey::getUpperValue(llvm::APInt &value) const
{
    if (const Range *range = getAsRange())
        value = range->getStaticUpperBound();
    else
        getAsExpr()->staticDiscreteValue(value);
}

bool ComponentKey::compareKeysU(const ComponentKey *X, const ComponentKey *Y)
{
    assert(X->isComparable() && Y->isComparable() && "Keys not comparable!");

    llvm::APInt boundX;
    llvm::APInt boundY;
    X->getLowerValue(boundX);
    Y->getLowerValue(boundY);
    return boundX.getZExtValue() < boundY.getZExtValue();
}

bool ComponentKey::compareKeysS(const ComponentKey *X, const ComponentKey *Y)
{
    assert(X->isComparable() && Y->isComparable() && "Keys not comparable!");

    llvm::APInt boundX;
    llvm::APInt boundY;
    X->getLowerValue(boundX);
    Y->getLowerValue(boundY);
    return boundX.getSExtValue() < boundY.getSExtValue();
}

//===----------------------------------------------------------------------===//
// ComponentKeyList

ComponentKeyList::ComponentKeyList(ComponentKey **keys, unsigned numKeys,
                                   Expr *expr)
  : keys(reinterpret_cast<ComponentKey**>(this + 1)),
    keyCount(numKeys),
    expr(expr)
{
    std::copy(keys, keys + numKeys, this->keys);
}

ComponentKeyList *ComponentKeyList::create(ComponentKey **keys,
                                           unsigned numKeys, Expr *expr)
{
    assert(numKeys != 0 && "At leaast one key must be present!");

    // Calculate the size of the needed ComponentKeyList and allocate the raw
    // memory.
    unsigned size = sizeof(ComponentKeyList) + sizeof(ComponentKey*) * numKeys;
    char *raw = new char[size];

    // Placement operator new using the classes constructor initializes the
    // internal structure.
    return new (raw) ComponentKeyList(keys, numKeys, expr);
}

void ComponentKeyList::dispose(ComponentKeyList *CKL)
{
    // Deallocate the associated AST nodes.
    delete CKL->expr;
    for (iterator I = CKL->begin(); I != CKL->end(); ++I)
        delete *I;

    // Cast CKL back to a raw pointer to char and deallocate.
    char *raw = reinterpret_cast<char*>(CKL);
    delete [] raw;
}

//===----------------------------------------------------------------------===//
// AggregateExpr

AggregateExpr::~AggregateExpr()
{
    for (pos_iterator I = pos_begin(); I != pos_end(); ++I)
        delete *I;

    for (kl_iterator I = kl_begin(); I != kl_end(); ++I)
        ComponentKeyList::dispose(*I);

    if (Expr *others = getOthersExpr())
        delete others;
}

bool AggregateExpr::empty() const
{
    return !(hasOthers() || hasKeyedComponents() || hasPositionalComponents());
}

unsigned AggregateExpr::numKeys() const
{
    unsigned result = 0;
    for (const_kl_iterator I = kl_begin(); I != kl_end(); ++I)
        result += (*I)->numKeys();
    return result;
}

bool AggregateExpr::hasStaticIndices() const
{
    if (hasKeyedComponents()) {
        // Only interrogate the first key since dynamicly sized aggregates can
        // only be specified using a single key.
        ComponentKey *key = keyedComponents.front()->getKey(0);
        return key->isStatic();
    }

    // This aggregate is purely positional.
    return true;
}

unsigned AggregateExpr::numComponents() const
{
    if (!hasStaticIndices())
        return 0;

    // FIXME: Use const_key_iterator when available.
    AggregateExpr *AE = const_cast<AggregateExpr*>(this);

    unsigned count = 0;
    key_iterator I = AE->key_begin();
    key_iterator E = AE->key_end();
    for ( ; I != E; ++I) {
        const ComponentKey *key = *I;
        if (const Range *range = key->getAsRange())
            count += range->length();
        else if (const DiscreteType *type = key->getAsDiscreteType())
            count += type->length();
        else {
            assert((key->denotesExpr() || key->denotesIdentifier()) &&
                   "Unexpected key!");
            count++;
        }
    }

    return count + numPositionalComponents();
}


