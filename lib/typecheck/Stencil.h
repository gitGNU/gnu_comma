//===-- typecheck/Stencil.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Helper classes used by the type checker to manage intermediate data
/// while consuming info from the parser.
///
/// While the parser drives the type checker, contexts are established to begin
/// the processing of complex constructs.  For example, function declarations
/// are processed by notifying the type checker with a call to
/// ParseClient::beginFunctionDeclaration(), followed by parameter and return
/// type processing.  We do not want to maintain incomplete AST nodes to hold
/// onto the intermediate data between callbacks.  Thus, ASTStencil's are used
/// to hold onto the necessary bits until the type checker can complete its
/// analysis and form a proper AST node.
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_STENCIL_HDR_GUARD
#define COMMA_TYPECHECK_STENCIL_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Expr.h"

#include "llvm/ADT/SmallVector.h"

#include <utility>

namespace comma {

//===----------------------------------------------------------------------===//
// ASTStencil
//
/// Common base class for stencils.  Provides encapsulation of identifier and
/// location information, as well as state for marking a stencil as invalid.
class ASTStencil {

public:
    virtual ~ASTStencil() { }

    ASTStencil() { reset(); }

    /// Initialize this stencil with the given location.
    void init(IdentifierInfo *name, Location loc) {
        this->name = name;
        this->location = loc;
    }

    /// Reset this stencil to its default state.
    virtual void reset() {
        validFlag = true;
        subBits = 0;
        name = 0;
        location = Location();
    }

    /// Returns the defining identifier associated with this stencil.
    IdentifierInfo *getIdInfo() const { return name; }

    /// Returns the location associated with this stencil.
    Location getLocation() const { return location; }

    /// Returns true if this stencil is valid.
    bool isValid() const { return validFlag; }

    /// Returns true if this stencil is invalid.
    bool isInvalid() const { return !validFlag; }

    /// Marks this stencil as invalid.
    void markInvalid() { validFlag = false; }

protected:
    IdentifierInfo *name;
    Location location;
    unsigned validFlag : 1;
    unsigned subBits : 8*sizeof(unsigned) - 1;
};

//===----------------------------------------------------------------------===//
//  ASTStencilReseter
//
/// A small class which automatically calls the given stencils reset method when
/// its constructor runs.
class ASTStencilReseter {
public:
    ASTStencilReseter(ASTStencil &stencil) : stencil(stencil) { }

    ~ASTStencilReseter() { stencil.reset(); }
private:
    ASTStencil &stencil;
};

//===----------------------------------------------------------------------===//
//  EnumDeclStencil
//
/// A stencil to hold enumeration declaration info.
class EnumDeclStencil : public ASTStencil {

public:
    EnumDeclStencil() { reset(); }

    void reset() {
        ASTStencil::reset();
        elements.clear();
    }

    /// A std::pair is used to represent enumeration literals.
    typedef std::pair<IdentifierInfo*, Location> IdLocPair;

    /// Adds an enumeration literal to this stencil, given the defining
    /// identifier for the literal and its location.
    void addElement(IdentifierInfo *name, Location loc) {
        elements.push_back(IdLocPair(name, loc));
    }

    /// Returns the number of enumeration literals contained in this stencil.
    unsigned numElements() const { return elements.size(); }

    /// Returns a pair representing the n'th enumeration literal.
    IdLocPair getElement(unsigned i) {
        assert(i < numElements() && "Index out of range!");
        return elements[i];
    }

    /// Type used to hold the enumeration literal pairs.
    typedef llvm::SmallVector<IdLocPair, 8> ElemVec;

    /// Returns a direct reference to the literal pair vector.
    ElemVec &getElements() { return elements; }
    const ElemVec &getElements() const { return elements; }

    //@{
    /// Iterators over the elements of this enum stencil.
    typedef ElemVec::iterator elem_iterator;
    elem_iterator begin_elems() { return elements.begin(); }
    elem_iterator end_elems() { return elements.end(); }
    //@}

    /// Mark the enumeration as being a character type.
    void markAsCharacterType() { subBits = 1; }

    /// Returns true if this stencil denotes a character enumeration.
    bool isCharacterType() const { return subBits == 1; }

private:
    ElemVec elements;           ///< Vector of Id/Loc pairs for each element.
};

//===----------------------------------------------------------------------===//
//  SRDeclStencil
//
/// A stencil to represent a subroutine declaration.
class SRDeclStencil : public ASTStencil {

public:
    SRDeclStencil() { reset(); }

    enum StencilKind {
        UNKNOWN_Stencil,        ///< An uninitialized stencil.
        FUNCTION_Stencil,       ///< A function stencil.
        PROCEDURE_Stencil       ///< A procedure stencil.
    };

    void init(IdentifierInfo *name, Location loc, StencilKind kind) {
        ASTStencil::init(name, loc);
        this->subBits = kind;
    }

    void reset() {
        ASTStencil::reset();
        returnTy = 0;
        subBits = UNKNOWN_Stencil;
        params.clear();
    };

    /// Returns true if this stencil denotes a subroutine.
    bool denotesProcedure() const { return subBits == PROCEDURE_Stencil; }

    /// Returns true if this stencil denotes a function.
    bool denotesFunction() const { return subBits == FUNCTION_Stencil; }

    /// Adds a parameter declaration to this stencil.
    void addParameter(ParamValueDecl *param) { params.push_back(param); }

    /// Returns the number of parameters associated with this stencil.
    unsigned numParameters() const { return params.size(); }

    /// Returns the i'th parameter asscoiated with this stencil.
    ParamValueDecl *getParameter(unsigned i) {
        assert(i < numParameters() && "Index out of range!");
        return params[i];
    }

    /// Container used to hold the subroutine parameters.
    typedef llvm::SmallVector<ParamValueDecl*, 8> ParamVec;

    /// Returns a direct reference to the vector of parameters.
    ParamVec &getParams() { return params; }
    const ParamVec &getParams() const { return params; }

    //@{
    /// Iterators over the parameters.
    typedef ParamVec::iterator param_iterator;
    param_iterator begin_params() { return params.begin(); }
    param_iterator end_params() { return params.end(); }
    //@}

    /// Sets the return type for this stencil.  The stencil must have been
    /// initialized as a function stencil.
    void setReturnType(TypeDecl *retTy) {
        assert(denotesFunction() && "Wrong type of stencil for return type!");
        returnTy = retTy;

    }

    /// Returns the return type of this stencil, or null if no return type info
    /// has been associated.
    TypeDecl *getReturnType() { return returnTy; }

private:
    ParamVec params;            ///< Parameter declarations.
    TypeDecl *returnTy;         ///< The return type or null.
};

} // end comma namespace.

#endif
