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

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"

namespace comma {

class Expr;
class SubType;

//===----------------------------------------------------------------------===//
// Constraint.
//
/// This class is the root of a small hierarchy which models the constraints
/// that can be imposed on SubType nodes.
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

class RangeConstraint : public Constraint {

public:
    RangeConstraint(const llvm::APInt low, const llvm::APInt high)
        : Constraint(Range_Constraint),
          low(low), high(high) { }

    const llvm::APInt &getLowerBound() const { return low; }
    const llvm::APInt &getUpperBound() const { return high; }

    // Support isa and dyn_cast.
    static bool classof(const RangeConstraint *constraint) { return true; }
    static bool classof(const Constraint *constraint) {
        return constraint->getKind() == Range_Constraint;
    }

private:
    llvm::APInt low;
    llvm::APInt high;
};

//===----------------------------------------------------------------------===//
// IndexConstraint

class IndexConstraint : public Constraint {

    typedef llvm::SmallVector<SubType*, 4> ConstraintVec;

public:
    IndexConstraint(SubType **constraints, unsigned numConstraints)
        : Constraint(Index_Constraint),
          indexConstraints(constraints, constraints + numConstraints) { }

    template <class Iter>
    IndexConstraint(Iter start, Iter end)
        : Constraint(Index_Constraint),
          indexConstraints(start, end) { }

    unsigned numConstraints() const { return indexConstraints.size(); }

    SubType *getConstraint(unsigned i) const {
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
