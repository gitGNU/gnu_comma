//===-- ast/Type.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_TYPE_HDR_GUARD
#define COMMA_AST_TYPE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/AstRewriter.h"
#include "comma/basic/ParameterModes.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/Support/Casting.h"

namespace comma {

//===----------------------------------------------------------------------===//
// Type

class Type : public Ast {

public:
    virtual ~Type() { }

    static bool classof(const Type *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesType();
    }

    virtual bool equals(const Type *type) const { return type == this; }

protected:
    Type(AstKind kind) : Ast(kind) {
        assert(this->denotesType());
    }
};

//===----------------------------------------------------------------------===//
// ModelType

class ModelType : public Type {

public:
    virtual ~ModelType() { }

    IdentifierInfo *getIdInfo() const { return idInfo; }

    // Returns a c-string representing the name of this model, or NULL if this
    // model is anonymous.
    const char *getString() const {
        return getIdInfo()->getString();
    }

    // Suport isa and dyn_cast.
    static bool classof(const ModelType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelType();
    }

    ModelDecl *getDeclaration() const { return declaration; }

protected:
    // FIXME:  We can get rid of the IdInfo and just refer to the decl.
    ModelType(AstKind kind, IdentifierInfo *idInfo, ModelDecl *decl)
        : Type(kind),
          idInfo(idInfo),
          declaration(decl)
        { assert(this->denotesModelType()); }

    IdentifierInfo *idInfo;
    ModelDecl *declaration;
};

//===----------------------------------------------------------------------===//
// SignatureType

class SignatureType : public ModelType, public llvm::FoldingSetNode {

public:
    Sigoid *getDeclaration() const;

    SignatureDecl *getSignature() const;

    VarietyDecl *getVariety() const;

    // Returns true if this type is a variety instance.
    bool isParameterized() const { return getVariety() != 0; }

    // Returns the number of arguments used to define this type.  When the
    // supporting declaration is a signature, the arity is zero.
    unsigned getArity() const;

    // Returns the i'th actual parameter.  This function asserts if its argument
    // is out of range.
    DomainType *getActualParameter(unsigned n) const;

    typedef DomainType **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    // Called by VarietyDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, DomainType **args, unsigned numArgs);

    static bool classof(const SignatureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureType;
    }

private:
    friend class SignatureDecl;
    friend class VarietyDecl;

    SignatureType(SignatureDecl *decl);

    SignatureType(VarietyDecl *decl, DomainType **args, unsigned numArgs);

    // If the supporting declaration is a variety, then this array contains the
    // actual arguments defining this instance.
    DomainType **arguments;
};

//===----------------------------------------------------------------------===//
// ParameterizedType
//
// Base class for both functor and variety types.

class ParameterizedType : public ModelType {

public:
    virtual ~ParameterizedType() { }

    unsigned getArity() const { return numFormals; }

    // Returns the domain type representing the formal parameter.
    DomainType *getFormalDomain(unsigned i) const;

    // Returns the SignatureType which the formal parameter satisfies (or which
    // an actual parameter must satisfy).
    SignatureType *getFormalType(unsigned i) const;

    // Returns the IdentifierInfo which labels this formal parameter.
    IdentifierInfo *getFormalIdInfo(unsigned i) const;

    // Returns the index of the parameter corresponding to the given keyword,
    // or -1 if no such keyword exists.
    int getKeywordIndex(IdentifierInfo *keyword) const;

    static bool classof(const ParameterizedType *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_VarietyType || kind == AST_FunctorType;
    }

protected:
    ParameterizedType(AstKind         kind,
                      IdentifierInfo *idInfo,
                      ModelDecl      *decl,
                      DomainType    **formalArguments,
                      unsigned        arity);

    DomainType **formals;
    unsigned numFormals;
};

//===----------------------------------------------------------------------===//
// VarietyType
//
// These nodes represent the type of a parameterized signature.  In some sense,
// they do not represent real types -- they are incomplete until provided with a
// compatible set of actual arguments.  The main role of these types is to
// provide a handle for the purpose of lookup resolution.
//
// VarietyType nodes are always owned by their associated decl.
class VarietyType : public ParameterizedType {

public:
    ~VarietyType();

    VarietyDecl *getDeclaration() const;

    static bool classof(const VarietyType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_VarietyType;
    }

private:
    friend class VarietyDecl;

    VarietyType(DomainType **formalArguments,
                VarietyDecl *variety,
                unsigned     arity);
};

//===----------------------------------------------------------------------===//
// FunctorType
//
// These nodes represent the type of a parameterized domain and serve
// essentially the same purpose of VarietyType nodes.  Again, FunctorType's do
// not represent real types (they are incomplete until provided with a
// compatible set of actual arguments), and are owned by their associated decl.
class FunctorType : public ParameterizedType {

public:
    ~FunctorType();

    FunctorDecl *getDeclaration() const;

    static bool classof(const FunctorType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorType;
    }

private:
    friend class FunctorDecl;

    FunctorType(DomainType **formalArguments,
                FunctorDecl *functor,
                unsigned     arity);
};

//===----------------------------------------------------------------------===//
// DomainType
class DomainType : public ModelType, public llvm::FoldingSetNode {

public:
    // Creates a domain type representing the given domain declaration.
    DomainType(DomainDecl *decl);

    // Creates a domain type representing an instance of the given functor
    // declaration.
    DomainType(FunctorDecl *decl, DomainType **args, unsigned numArgs);

    // Creates a domain type representing the given abstract domain.
    DomainType(AbstractDomainDecl *decl);

    // Creates a domain type representing the % node of the given model.
    static DomainType *getPercent(IdentifierInfo *percentInfo,
                                  ModelDecl      *model);

    // Returns true if this node is a percent node.
    bool denotesPercent() const;

    // Returns the number of arguments used to define this type.  When the
    // supporting declaration is a domain, the arity is zero.  When the
    // supporting declaration is a functor, this method returns the number of
    // actual parameters.
    unsigned getArity() const;

    // Returns the i'th actual parameter.  This function asserts if its argument
    // is out of range,
    DomainType *getActualParameter(unsigned n) const;

    // Returns true if this domain type is an instance of some functor.
    bool isParameterized() const { return arguments != 0; }

    typedef DomainType **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a domoid.
    Domoid *getDomoidDecl() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a domain.
    DomainDecl *getDomainDecl() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a functor.
    FunctorDecl *getFunctorDecl() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is an abstract domain.
    AbstractDomainDecl *getAbstractDecl() const;

    // Returns true if the underlying declaration is an AbstractDomainDecl.
    bool isAbstract() const;

    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    // Called by FunctorDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, DomainType **args, unsigned numArgs);

    // Support isa and dyn_cast.
    static bool classof(const DomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainType;
    }

private:
    // This constructor is called by getPercent() to create a percent node.
    DomainType(IdentifierInfo *percentId, ModelDecl *model);

    // If the supporting domain is a functor, then this array contains the
    // actual arguments defining this instance.
    DomainType **arguments;
};

//===----------------------------------------------------------------------===//
// SubroutineType
class SubroutineType : public Type {

protected:
    // This constructor produces a subroutine type where the parameter modes are
    // set to MODE_DEFAULT.
    SubroutineType(AstKind          kind,
                   IdentifierInfo **formals,
                   DomainType     **argTypes,
                   unsigned         numArgs);

    // Constructor where each parameter mode can be specified.
    SubroutineType(AstKind          kind,
                   IdentifierInfo **formals,
                   DomainType     **argTypes,
                   ParameterMode   *modes,
                   unsigned         numArgs);

public:
    // Returns the number of arguments accepted by this type.
    unsigned getArity() const { return numArgs; }

    // Returns the type of the i'th parameter.
    DomainType *getArgType(unsigned i) const;

    // Returns the i'th keyword for this type.
    IdentifierInfo *getKeyword(unsigned i) const {
        assert(i < getArity() && "Index out of range!");
        return keywords[i];
    }

    int getKeywordIndex(IdentifierInfo *key) const;

    // Returns the i'th parameter mode for this type.
    ParameterMode getParameterMode(unsigned i) const;

    // Sets the i'th parameter mode.  This method will assert if this subroutine
    // denotes a function type and the mode is `out' or `in out'.
    void setParameterMode(ParameterMode mode, unsigned i);

    // Returns an array of IdentifierInfo's corresponding to the keyword set for
    // this type, or 0 if there are no parameters.  This function is intended to
    // be used to simplify construction of new SubroutineType nodes, not as
    // general purpose accessor.
    IdentifierInfo **getKeywordArray() const;

    // Returns true if the keywords of the given type match exactly those of
    // this type.  The arity of both subroutine types must match for this
    // function to return true.
    bool keywordsMatch(const SubroutineType *routineType) const;

    // Returns true if this type is equal to the given subroutine type.  Both
    // this type and the target must both be function or procedure types, the
    // arity, argument, and (in the case of functions) the return types must
    // match.  Actual argument keywords are not considered when testing for
    // equality.
    bool equals(const Type *type) const;

    // Support isa and dyn_cast.
    static bool classof(const SubroutineType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubroutineType();
    }

private:
    // We munge the supplied parameter type pointers and store the mode
    // associations in the lower two bits.
    typedef llvm::PointerIntPair<DomainType*, 2> ParamInfo;

    IdentifierInfo **keywords;
    ParamInfo       *parameterInfo;
    unsigned         numArgs;
};

//===----------------------------------------------------------------------===//
// FunctionType
class FunctionType : public SubroutineType {

public:
    FunctionType(IdentifierInfo **formals,
                 DomainType     **argTypes,
                 unsigned         numArgs,
                 DomainType      *returnType)
        : SubroutineType(AST_FunctionType, formals, argTypes, numArgs),
          returnType(returnType) { }

    // Returns the result type of this function.
    DomainType *getReturnType() const { return returnType; }

    // Support isa and dyn_cast.
    static bool classof(const FunctionType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionType;
    }

private:
    DomainType *returnType;
};

//===----------------------------------------------------------------------===//
// ProcedureType
class ProcedureType : public SubroutineType {

public:
    ProcedureType(IdentifierInfo **formals,
                  DomainType     **argTypes,
                  unsigned         numArgs)
        : SubroutineType(AST_ProcedureType, formals, argTypes, numArgs) { }

    // Support isa and dyn_cast.
    static bool classof(const ProcedureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureType;
    }
};

} // End comma namespace

#endif
