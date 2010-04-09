//===-- ast/PackageRef.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_PACKAGEREF_HDR_GUARD
#define COMMA_AST_PACKAGEREF_HDR_GUARD

namespace comma {

/// \class
/// \brief A thin wrapper packaging together a PkgInstanceDecl and a Location.
class PackageRef : public Ast {

public:
    PackageRef(Location loc, PkgInstanceDecl *package)
        : Ast(AST_PackageRef), loc(loc), package(package) { }

    /// Returns the location associated with this PackageRef.
    Location getLocation() const { return loc; }

    //@{
    /// Returns the package instance declaration this node references.
    const PkgInstanceDecl *getPackageInstance() const { return package; }
    PkgInstanceDecl *getPackageInstance() { return package; }
    //@}

    /// Sets the package instance associated with this reference.
    void setPackageInstance(PkgInstanceDecl *package) {
        this->package = package;
    }

    /// Returns the defining identifier of this associated package instance.
    IdentifierInfo *getIdInfo() const { return package->getIdInfo(); }

    /// Returns the defining identifier of the associated package as a C-string.
    const char *getString() const { return package->getString(); }

    // Support isa/dyn_cast.
    static bool classof(const PackageRef *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PackageRef;
    }

private:
    Location loc;
    PkgInstanceDecl *package;
};

} // end comma namespace.

#endif
