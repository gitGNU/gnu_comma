//===-- ast/Qualifier.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the Qualifier class.
///
/// Qualifier nodes do not belong to any of the major branches of the Ast
/// hierarchy. They do not denote resolved names.  They simply model the prefix
/// of a name.  For example, for the syntax <tt>A::B::C</tt>, the qualifier
/// corresponds to <tt>A::B::</tt>.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_QUALIFIER_HDR_GUARD
#define COMMA_AST_QUALIFIER_HDR_GUARD

namespace comma {

class Qualifier : public Ast {

public:
    Qualifier(DomainTypeDecl *qualifier,  Location loc)
        : Ast(AST_Qualifier) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    Qualifier(SigInstanceDecl *qualifier, Location loc)
        : Ast(AST_Qualifier) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    Qualifier(EnumerationDecl *qualifier, Location loc)
        : Ast(AST_Qualifier) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    void addQualifier(DomainTypeDecl *qualifier, Location loc) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    void addQualifier(SigInstanceDecl *qualifier, Location loc) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    void addQualifier(EnumerationDecl *qualifier, Location loc) {
        qualifiers.push_back(QualPair(qualifier, loc));
    }

    unsigned numQualifiers() const { return qualifiers.size(); }

    typedef std::pair<Decl*, Location> QualPair;

    QualPair getQualifier(unsigned n) const {
        assert(n < numQualifiers() && "Index out of range!");
        return qualifiers[n];
    }

    /// Returns the base (most specific) declaration of this qualifier.
    Decl *getBaseDecl() { return qualifiers.back().first; }

    /// Returns the location of the most specific component of this qualifier.
    ///
    /// This location corresponds to the components provided by the resolve
    /// methods.
    Location getBaseLocation() const { return qualifiers.back().second; }

    /// Returns the base (most specific) declaration of this qualifier, casted
    /// to the given type, or 0 if the base declaration is not of the requested
    /// type.
    template <class T>
    T *resolve() { return llvm::dyn_cast<T>(qualifiers.back().first); }

    /// Returns the declarative region which this qualifier denotes.
    ///
    /// If the base declaration of this qualifier is a DomainTypeDecl or
    /// EnumerationDecl, the result is just the decl downcast to a region.  Then
    /// the qualifier denotes a SigInstanceDecl, the region is the PercentDecl
    /// of the underlying sigoid.
    DeclRegion *resolveRegion();

private:
    typedef llvm::SmallVector<QualPair, 2> QualVector;
    QualVector qualifiers;

public:
    typedef QualVector::iterator iterator;
    iterator begin() { return qualifiers.begin(); }
    iterator end()   { return qualifiers.end(); }

    typedef QualVector::const_iterator const_iterator;
    const_iterator begin() const { return qualifiers.begin(); }
    const_iterator end()   const { return qualifiers.end(); }

    static bool classof(const Qualifier *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_Qualifier;
    }
};

} // end comma namespace.

#endif
