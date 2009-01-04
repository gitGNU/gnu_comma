//===-- ast/Decl.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECL_HDR_GUARD
#define COMMA_AST_DECL_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Type.h"
#include "comma/ast/ParamList.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <vector>

namespace comma {

//===----------------------------------------------------------------------===//
// Decl.
//
// Decl nodes represent declarations within a Comma program.
class Decl : public Ast {

public:
    virtual ~Decl() { };

    // Retuns the IdentifierInfo object associated with this decl, or NULL if
    // this is an anonymous decl.
    IdentifierInfo *getIdInfo() const { return idInfo; }

    // Returns the name of this decl as a c string, or NULL if this is an
    // anonymous decl.
    const char *getString() const {
        return idInfo ? idInfo->getString() : 0;
    }

    // Returns true if this decl is anonymous.  Currently, the only anonymous
    // models are the "principle signatures" of a domain.
    bool isAnonymous() const { return idInfo == 0; }

    // Support isa and dyn_cast.
    static bool classof(const Decl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDecl();
    }

protected:
    Decl(AstKind kind, IdentifierInfo *info = 0) : Ast(kind), idInfo(info) {
        assert(this->denotesDecl());
    }

    IdentifierInfo *idInfo;
};

//===----------------------------------------------------------------------===//
// TypeDecl
//
// The root of the comma type hierarchy,
class TypeDecl : public Decl {

public:
    virtual ~TypeDecl() { }

    // Support isa and dyn_cast.
    static bool classof(const TypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesTypeDecl();
    }

protected:
    TypeDecl(AstKind kind, IdentifierInfo *info = 0)
        : Decl(kind, info) {
        assert(this->denotesTypeDecl());
    }
};

//===----------------------------------------------------------------------===//
// ModelDecl
//
// Models represent those attributes and characteristics which both signatures
// and domains share.
class ModelDecl : public TypeDecl {

public:
    virtual ~ModelDecl() { }

    // Access the location info of this node.
    Location getLocation() const { return location; }

    // Returns true if this model is parameterized.
    bool isParameterized() const {
        return kind == AST_VarietyDecl || kind == AST_FunctorDecl;
    }

    PercentType *getPercent() { return percent; }

    ParameterizedModel *getParameterizedModel();

    bool isTypeComplete() const { return typeComplete; }
    void setTypeComplete() { typeComplete = true; }


    struct Assumption {
        Assumption(SignatureType *type, Location loc)
            : type(type), loc(loc) { }
        SignatureType *type;
        Location loc;
    };

    void addAssumption(SignatureType *type, Location loc) {
        assumptions.push_back(Assumption(type, loc));
    }

    typedef std::vector<Assumption>::const_iterator assumption_iterator;
    assumption_iterator beginAssumptions() const { return assumptions.begin(); }
    assumption_iterator endAssumptions()   const { return assumptions.end(); }

    // Support isa and dyn_cast.
    static bool classof(const ModelDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelDecl();
    }

protected:
    // Creates an anonymous Model.  Note that anonymous models do not have valid
    // location information as they never correspond to a source level construct.
    ModelDecl(AstKind kind);

    ModelDecl(AstKind kind, IdentifierInfo *info, const Location &loc);

    // Location information provided by the constructor.
    Location location;

    // Percent node for this decl.
    PercentType *percent;

    bool typeComplete;

    std::vector<Assumption> assumptions;
};

//===----------------------------------------------------------------------===//
// ParameterizedModel
//
// Mixin class for signatures and domains which are parameterized.
class ParameterizedModel {

public:
    virtual ~ParameterizedModel() { }

    // Adds a formal parameter to the model.  The order in which this function
    // is called determines the order of the formal parameter list.
    void addParameter(IdentifierInfo *formal,
                      SignatureType *type, DomainType *dom) {
        params.addParameter(formal, type);
        formalDomains.push_back(dom);
    }

    unsigned getArity() const { return formalDomains.size(); }

    DomainType *getFormalDomain(unsigned i) const {
        assert(i < getArity());
        return formalDomains[i];
    }

    // Returns the parameter list of this model.
    ParameterList *getFormalParams() { return &params; }
    const ParameterList *getFormalParams() const { return &params; }

    virtual ModelType *getCorrespondingType(ModelType **args,
                                            unsigned numArgs) = 0;

protected:
    // The parameterization of this model.
    ParameterList params;
    std::vector<DomainType *> formalDomains;
};

//===----------------------------------------------------------------------===//
// Sigoid
//
// This is the common base class for "signature like" objects: i.e. signatures
// and varieties.
class Sigoid : public ModelDecl {

public:
    Sigoid(AstKind kind) : ModelDecl(kind) { }

    Sigoid(AstKind kind, IdentifierInfo *idInfo, Location loc)
        : ModelDecl(kind, idInfo, loc) { }

    virtual ~Sigoid() { }
protected:
    typedef llvm::SmallPtrSet<SignatureType*, 4> DirectSigTable;
    DirectSigTable directSupers;

public:
    void addSupersignature(SignatureType *supersignature);

    typedef DirectSigTable::const_iterator sig_iterator;
    sig_iterator beginDirectSupers() const { return directSupers.begin(); }
    sig_iterator endDirectSupers()   const { return directSupers.end(); }

    static bool classof(const Sigoid *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_SignatureDecl || kind == AST_VarietyDecl;
    }
};

//===----------------------------------------------------------------------===//
// SignatureDecl
//
// This class defines signature declarations.
//
// FIXME: Signatures can be anonymous.  But anonymous signatures are used only
// to represent the priciple signatures of DomainDecl's.  The correct fix seems
// to be to have a seperated class AnonymousSignature, or perhaps even
// PrincipleSignature, which does not inherit from NamedDecl.
class SignatureDecl : public Sigoid {

    typedef std::pair<IdentifierInfo *, Ast *> DeclEntry;
    typedef std::vector<DeclEntry> DeclTable;
    DeclTable declTable;

public:
    // Creates an anonymous signature.
    SignatureDecl();
    SignatureDecl(IdentifierInfo *name, const Location &loc);

    // Adds a declaration with the given name and associated type to this
    // signature.  This is the only method available for adding a declaration to
    // a signature.  The sequence of calls made to this method is important, as
    // it implicitly defines the declaration order.
    void addDeclaration(IdentifierInfo *name, Ast *type) {
        declTable.push_back(DeclEntry(name, type));
    }

    // Iterators providing access the declarations assoiciated with this
    // signature.  The order of iteration corresponds to the order of
    // declaration (as implied by the sequence of calls make to
    // add_declaration).  These iterators return elements of type
    // std::pair<Identifier*, Ast*>, where the Identifier names the declaration
    // and the Ast is a type node.
    typedef DeclTable::const_iterator decl_iterator;
    decl_iterator beginDeclarations() const { return declTable.begin(); }
    decl_iterator endDeclarations()   const { return declTable.end(); }

    SignatureType *getCorrespondingType();

    // Support for isa and dyn_cast.
    static bool classof(const SignatureDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureDecl;
    }

private:
    // The unique type representing this signature (accessible via
    // getCorrespondingType()).
    SignatureType *canonicalType;
};

//===----------------------------------------------------------------------===//
// VarietyDecl
//
// Repesentation of parameterized signatures.
class VarietyDecl : public Sigoid, public ParameterizedModel {

public:
    VarietyDecl(IdentifierInfo *info, Location loc)
        : Sigoid(AST_VarietyDecl, info, loc) { }

    SignatureType *getCorrespondingType(ModelType **args, unsigned numArgs);

    typedef llvm::FoldingSet<SignatureType>::iterator type_iterator;
    type_iterator beginTypes() { return types.begin(); }
    type_iterator endTypes() { return types.end(); }

    // Support for isa and dyn_cast.
    static bool classof(const VarietyDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_VarietyDecl;
    }

private:
    mutable llvm::FoldingSet<SignatureType> types;
};


//===----------------------------------------------------------------------===//
// Domoid
//
// This is the common base class for domain-like objects: i.e. domains
// and functors.
class Domoid : public ModelDecl {

public:
    virtual ~Domoid() { }

    SignatureType *getPrincipleSignature() const { return principleSignature; }

    static bool classof(const Domoid *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_DomainDecl || kind == AST_FunctorDecl;
    }

protected:
    Domoid(AstKind kind,
           IdentifierInfo *idInfo,
           Location loc,
           SignatureType *signature = 0);

    SignatureType *principleSignature;
};

//===----------------------------------------------------------------------===//
// DomainDecl
//
class DomainDecl : public Domoid {

public:
    DomainDecl(IdentifierInfo *name,
               const Location &loc, SignatureType *sig = 0);

    DomainType *getCorrespondingType();

    // Support for isa and dyn_cast.
    static bool classof(const DomainDecl *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_DomainDecl || kind == AST_FunctorDecl;
    }

protected:
    // For use by subclasses.
    DomainDecl(AstKind kind, IdentifierInfo *info, Location loc);

private:
    DomainType *canonicalType;
};

//===----------------------------------------------------------------------===//
// FunctorDecl
//
// Repesentation of parameterized signatures.
class FunctorDecl : public Domoid, public ParameterizedModel {

public:
    FunctorDecl(IdentifierInfo *info, Location loc)
        : Domoid(AST_FunctorDecl, info, loc) { }

    DomainType *getCorrespondingType(ModelType **args, unsigned numArgs);

    // Support for isa and dyn_cast.
    static bool classof(const FunctorDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorDecl;
    }

private:
    mutable llvm::FoldingSet<DomainType> types;
};


} // End comma namespace

#endif
