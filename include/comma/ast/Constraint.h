//===-- ast/Constraint.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the Constraint hierarchy.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_CONSTRAINT_HDR_GUARD
#define COMMA_AST_CONSTRAINT_HDR_GUARD

#include "comma/ast/Range.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"

namespace comma {

class Expr;
class DiscreteType;

//===----------------------------------------------------------------------===//
// Constraint.
//
/// This class is the root of a small hierarchy which models the constraints
/// that can be imposed on subtype nodes.
class Constraint {

public:
    enum ConstraintKind {
        Range_Constraint,
        Index_Constraint
    };

    virtual ~Constraint() { }

    ConstraintKind getKind() const { return kind; }

    static bool classof(const Constraint *node) { return true; }

protected:
    Constraint(ConstraintKind kind) : kind(kind) { }

private:
    ConstraintKind kind;
};

//===----------------------------------------------------------------------===//
// RangeConstraint.

class RangeConstraint : public Constraint, public Range {

public:
    RangeConstraint(Expr *lower, Expr *upper)
        : Constraint(Range_Constraint),
          Range(lower, upper) { }

    // Support isa and dyn_cast.
    static bool classof(const RangeConstraint *constraint) { return true; }
    static bool classof(const Constraint *constraint) {
        return constraint->getKind() == Range_Constraint;
    }
};

//===----------------------------------------------------------------------===//
// IndexConstraint

class IndexConstraint : public Constraint {

    typedef llvm::SmallVector<DiscreteType*, 4> ConstraintVec;

public:
    IndexConstraint(DiscreteType **constraints, unsigned numConstraints)
        : Constraint(Index_Constraint),
          indexConstraints(constraints, constraints + numConstraints) { }

    template <class Iter>
    IndexConstraint(Iter start, Iter end)
        : Constraint(Index_Constraint),
          indexConstraints(start, end) { }

    IndexConstraint(const IndexConstraint &constraint)
        : Constraint(Index_Constraint),
          indexConstraints(constraint.begin(), constraint.end()) { }

    unsigned numConstraints() const { return indexConstraints.size(); }

    const DiscreteType *getConstraint(unsigned i) const {
        assert(i < numConstraints() && "Index out of range!");
        return indexConstraints[i];
    }

    DiscreteType *getConstraint(unsigned i) {
        assert(i < numConstraints() && "Index out of range!");
        return indexConstraints[i];
    }

    typedef ConstraintVec::const_iterator iterator;
    iterator begin() const { return indexConstraints.begin(); }
    iterator end() const { return indexConstraints.end(); }

    static bool classof(const IndexConstraint *constraint) { return true; }
    static bool classof(const Constraint *constraint) {
        return constraint->getKind() == Index_Constraint;
    }

private:
    ConstraintVec indexConstraints;
};

} // end comma namespace.

#endif
