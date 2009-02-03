//===-- parser/Descriptors.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/IdentifierInfo.h"
#include "comma/basic/Location.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/ADT/SmallVector.h"

namespace comma {

class Node {

public:
    // Constructs an invalid node.
    Node() : payload(1) { }
    Node(void *ptr) : payload(reinterpret_cast<uintptr_t>(ptr)) { }

    static Node getInvalidNode() { return Node(); }

    // Returns true if this node is invalid.
    bool isInvalid() const { return payload & 1u; }

    // Returns true if this Node is valid.
    bool isValid() const { return !isInvalid(); }

    // Returns the pointer associated with this node cast to the supplied type.
    template <class T> static T *lift(Node &node) {
        return reinterpret_cast<T*>(node.payload & ~((uintptr_t)1));
    }

private:
    uintptr_t payload;
};

class Descriptor {

    /// The type used to hold nodes representing the formal parameters of this
    /// descriptor.
    typedef llvm::SmallVector<Node, 16> paramVector;

public:

    enum DescriptorKind {
        DESC_Empty,             ///< Kind of uninitialized descriptors.
        DESC_Signature,         ///< Signature descriptors.
        DESC_Domain,            ///< Domain descriptors.
        DESC_Function,          ///< Function descriptors.
        DESC_Procedure          ///< Procedure descriptors.
    };

    /// The default constructor initializes an empty descriptor.  One must call
    /// initialize() and provide a valid kind before the object can be used.
    Descriptor();

    /// Creates a descriptor object of the given kind.
    Descriptor(DescriptorKind kind);

    /// Sets this Descriptor to an uninitialized state allowing it to be reused
    /// to represent a new element.  On return, the kind of this descriptor is
    /// set to DESC_Empty.
    void clear();

    /// Clears the current state of this Descriptor and sets is type to the
    /// given kind.
    void initialize(DescriptorKind descKind) {
        clear();
        kind = descKind;
    }

    /// Associates the given identifier and location with this descriptor.
    void setIdentifier(IdentifierInfo *id, Location loc) {
        idInfo   = id;
        location = loc;
    }

    /// Returns the kind of this descriptor.
    DescriptorKind getKind() const { return kind; }

    /// Predicate functions for enquiering about this descriptors kind.
    bool isFunctionDescriptor()  const { return kind == DESC_Function; }
    bool isProcedureDescriptor() const { return kind == DESC_Procedure; }
    bool isSignatureDescriptor() const { return kind == DESC_Signature; }
    bool isDomainDescriptor()    const { return kind == DESC_Domain; }

    /// Retrives the identifier associated with this descriptor.
    IdentifierInfo *getIdInfo() const { return idInfo; }

    /// Retrives the location associated with this descriptor.
    Location getLocation() const { return location; }

    bool isValid() const { return !invalidFlag; }
    bool isInvalid() const { return invalidFlag; }
    void setInvalid() { invalidFlag = true; }

    /// Adds a Node to this descriptor representing the formal parameter of a
    /// model or subroutine.
    void addParam(Node param) { params.push_back(param); }

    /// Returns the number of parameters currently associated with this
    /// descriptor.
    unsigned numParams() const { return params.size(); }

    /// Returns true if parameters have been associated with this descriptor.
    bool hasParams() const { return numParams() > 0; }

    typedef paramVector::iterator paramIterator;
    paramIterator beginParams() { return params.begin(); }
    paramIterator endParams()   { return params.end(); }

    /// Sets the return type of this descriptor.  Return types can only be
    /// associated with function descriptors.  This method will assert if this
    /// descriptor is of any other kind.
    void setReturnType(Node node);

    /// Accesses the return type of this descriptor.  A call to this function is
    /// valid iff the descriptor denotes a function and setReturnType has been
    /// called.  If these conditions do not hold then this method will assert.
    Node getReturnType();

private:
    DescriptorKind  kind        : 8;
    bool            invalidFlag : 1;
    bool            hasReturn   : 1;
    IdentifierInfo *idInfo;
    Location        location;
    paramVector     params;
    Node            returnType;
};



} // End comma namespace.
