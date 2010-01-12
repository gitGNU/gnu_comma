//===-- CValue.h ---------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CVALUE_HDR_GUARD
#define COMMA_CODEGEN_CVALUE_HDR_GUARD

#include "llvm/ADT/PointerIntPair.h"
#include "llvm/Value.h"

namespace comma {

//===----------------------------------------------------------------------===//
// CValue
//
/// \class
///
/// The CValue class represents the result of evaluating a program entity for
/// its value.  Such objects are represented as either a single LLVM value, or a
/// pair of values representing the object and its bounds.
class CValue {

public:
    /// There are three kinds of values.  Simple values represent scalars and
    /// thin access pointers.  Aggregate values contain a pointer to the data
    /// and a representation of the bounds (either a struct or pointer-to
    /// struct).  Fat pointers are represented as a single pointer to structure
    /// type.
    enum Kind {
        Simple,
        Aggregate,
        Fat
    };

    /// Returns the first value associated with this CValue.
    llvm::Value *first() { return primary; }

    /// Returns the second value associated with this CValue.
    llvm::Value *second() { return secondary.getPointer(); }

    /// Returns true if this denotes a simple value.
    bool isSimple() const { return secondary.getInt() == Simple; }

    /// Returns true if this denotes an Aggregate value.
    bool isAggregate() const { return secondary.getInt() == Aggregate; }

    /// Returns true if this denotes a fat access value.
    bool isFat() const { return secondary.getInt() == Fat; }

    /// \name Static Constructors.
    //@{
    static CValue getSimple(llvm::Value *V1) {
        return CValue(V1, 0, Simple);
    }

    static CValue getAggregate(llvm::Value *V1, llvm::Value *V2) {
        return CValue(V1, V2, Aggregate);
    }

    static CValue getFat(llvm::Value *V1) {
        return CValue(V1, 0, Fat);
    }
    //@}

private:
    /// Private constructor for use by the static construction methods.
    CValue(llvm::Value *V1, llvm::Value *V2, Kind kind)
        : primary(V1), secondary(V2, kind) { }

    /// CValue objects are represented with a "clean" first pointer.  The kind
    /// of the CValue is then munged into the low order bits of the second
    /// pointer.
    llvm::Value *primary;
    llvm::PointerIntPair<llvm::Value*, 2, Kind> secondary;
};

} // end comma namespace.

#endif

