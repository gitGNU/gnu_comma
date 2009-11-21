//===-- codegen/BoundsEmitter.h ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Provides the BoundsEmitter class.
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_BOUNDSEMITTER_HDR_GUARD
#define COMMA_CODEGEN_BOUNDSEMITTER_HDR_GUARD

#include "CodeGenCapsule.h"
#include "CodeGenRoutine.h"
#include "comma/ast/AstBase.h"

namespace comma {

/// \class
///
/// \brief The BoundsEmitter class provides methods for the creation and
/// manipulation of bounds objects in LLVM IR.
///
/// Bounds are represented as LLVM structure types.  These structures contain
/// pairs of entries representing the bounds of an array.  Each pair of entries
/// is of an integer type, the exact width being determined by the type of the
/// associated array index.
class BoundsEmitter {

public:
    BoundsEmitter(CodeGenRoutine &CGR)
        : CGR(CGR),
          CG(CGR.getCodeGen()),
          CGT(CGR.getCGC().getTypeGenerator()) { }

    /// \brief Returns the LLVM type which represents the bounds of the given
    /// Comma array type.
    ///
    /// \note This is simply a convinience wrapper around
    /// CodeGenTypes::lowerArrayBounds().
    const llvm::StructType *getType(const ArrayType *arrTy);

    /// Returns the lower bound at the given index.
    static llvm::Value *getLowerBound(llvm::IRBuilder<> &Builder,
                                      llvm::Value *bounds, unsigned index) {
        llvm::Value *res;
        index = 2 * index;
        if (llvm::isa<llvm::PointerType>(bounds->getType())) {
            res = Builder.CreateConstInBoundsGEP2_32(bounds, 0, index);
            res = Builder.CreateLoad(res);
        }
        else
            res = Builder.CreateExtractValue(bounds, index);
        return res;
    }

    /// Returns the upper bound at the given index.
    static llvm::Value *getUpperBound(llvm::IRBuilder<> &Builder,
                                      llvm::Value *bounds, unsigned index) {
        llvm::Value *res;
        index = 2 * index + 1;
        if (llvm::isa<llvm::PointerType>(bounds->getType())) {
            res = Builder.CreateConstInBoundsGEP2_32(bounds, 0, index);
            res = Builder.CreateLoad(res);
        }
        else
            res = Builder.CreateExtractValue(bounds, index);
        return res;
    }

    /// Convenience method to pack the results of getLowerBound() and
    /// getUpperBound() into a std::pair.
    static std::pair<llvm::Value*, llvm::Value*>
    getBounds(llvm::IRBuilder<> &Builder, llvm::Value *bounds, unsigned index) {
        std::pair<llvm::Value*, llvm::Value*> res;
        res.first = getLowerBound(Builder, bounds, index);
        res.second = getUpperBound(Builder, bounds, index);
        return res;
    }

    /// Evaluates the range of the given scalar type and returns a bounds
    /// structure.
    llvm::Value *synthScalarBounds(llvm::IRBuilder<> &Builder,
                                   const DiscreteType *type);

    /// Evaluates the range of the given scalar type and returns the lower and
    /// upper bounds as a pair.
    std::pair<llvm::Value*, llvm::Value*>
    getScalarBounds(llvm::IRBuilder<> &Builder, const DiscreteType *type);

    /// Emits code which computes the length of the given bounds value.
    ///
    /// The returned Value is always an i32.  This may change in the future when
    /// LLVM supports i64 alloca's.
    llvm::Value *computeBoundLength(llvm::IRBuilder<> &Builder,
                                    llvm::Value *bounds, unsigned index);

    /// Emits code which computes the total length of the given bounds value.
    ///
    /// Like computeBoundLength(), this method returns an i32.
    llvm::Value *computeTotalBoundLength(llvm::IRBuilder<> &Builder,
                                         llvm::Value *bounds);

    /// Emits code which tests if the given bounds object has a null range at
    /// the given index.  The reuturn value is always an i1.
    llvm::Value *computeIsNull(llvm::IRBuilder<> &Builder,
                               llvm::Value *bounds, unsigned index);

    /// \brief Given an array type with statically constrained indices,
    /// synthesizes a constant LLVM structure representing the bounds of the
    /// array.
    ///
    /// If \p dst is non-null, the synthesized bounds are stored into the given
    /// location.
    llvm::Constant *synthStaticArrayBounds(llvm::IRBuilder<> &Builder,
                                           ArrayType *arrTy,
                                           llvm::Value *dst = 0);

    /// Constructs an LLVM structure object representing the bounds of the given
    /// aggregate expression.
    ///
    /// If \p dst is non-null the dynthesized bounds are stored into the given
    /// location.
    llvm::Value *synthAggregateBounds(llvm::IRBuilder<> &Builder,
                                      AggregateExpr *agg, llvm::Value *dst = 0);

private:
    CodeGenRoutine &CGR;        // Routine generator.
    CodeGen &CG;                // Code generation context.
    CodeGenTypes &CGT;          // Type generator.
};

} // end comma namespace.

#endif
