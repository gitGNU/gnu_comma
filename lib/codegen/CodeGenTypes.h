//===-- codegen/CodeGenTypes.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENTYPES_HDR_GUARD
#define COMMA_CODEGEN_CODEGENTYPES_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/DerivedTypes.h"

namespace comma {

class CGContext;
class CodeGen;

/// Lowers various Comma AST nodes to LLVM types.
///
/// As a rule, Comma type nodes need not in and of themselves provide enough
/// information to lower them directly to LLVM IR.  Thus, declaration nodes are
/// often needed so that the necessary information can be extracted from the
/// AST.
class CodeGenTypes {

public:
    CodeGenTypes(CodeGen &CG, DomainInstanceDecl *context)
        : CG(CG), topScope(rewrites), context(context) {
        if (context)
            addInstanceRewrites(context);
    };

    const llvm::Type *lowerType(const Type *type);

    const llvm::Type *lowerDomainType(const DomainType *type);

    const llvm::IntegerType *lowerEnumType(const EnumerationType *type);

    const llvm::FunctionType *lowerSubroutine(const SubroutineDecl *decl);

    const llvm::IntegerType *lowerDiscreteType(const DiscreteType *type);

    const llvm::ArrayType *lowerArrayType(const ArrayType *type);

    const llvm::StructType *lowerRecordType(const RecordType *type);

    const llvm::Type *lowerIncompleteType(const IncompleteType *type);

    const llvm::PointerType *lowerAccessType(const AccessType *type);

    /// Returns the structure type used to hold the bounds of an unconstrained
    /// array.
    const llvm::StructType *lowerArrayBounds(const ArrayType *arrTy);

    /// Returns the structure type used to hold the bounds of the given scalar
    /// type.
    const llvm::StructType *lowerScalarBounds(const DiscreteType *type);

    /// Returns the structure type used to hold the bounds of the given range.
    const llvm::StructType *lowerRange(const Range *range);

    /// Returns the index into an llvm structure type that should be used to GEP
    /// the given component.
    unsigned getComponentIndex(const ComponentDecl *component);

    /// Returns the alignment of the given llvm type according to the targets
    /// ABI conventions.
    unsigned getTypeAlignment(const llvm::Type *type) const;

    /// Returns the size of the given llvm type in bytes.
    uint64_t getTypeSize(const llvm::Type *type) const;

    /// Resolves the given type.
    ///
    /// In the case a domain types this method resolves the underlying
    /// representation.  In the case of incomplete types this method resolves
    /// the completion.
    ///
    /// FIXME: This method is a close cousin to CGR's resolveType.  It might be
    /// best remove CGR's version and use this one everywhere.
    const PrimaryType *resolveType(const Type *type);

private:
    CodeGen &CG;

    typedef llvm::ScopedHashTable<const Type*, const Type*> RewriteMap;
    typedef llvm::ScopedHashTableScope<const Type*, const Type*> RewriteScope;
    RewriteMap rewrites;
    RewriteScope topScope;

    DomainInstanceDecl *context;

    /// Map from ComponentDecl's to the associated index within an llvm
    /// structure.
    typedef llvm::DenseMap<const ComponentDecl*, unsigned> ComponentIndexMap;
    ComponentIndexMap ComponentIndices;

    /// Map from Comma to LLVM types.
    ///
    /// This map is used to provide fast lookup for previously lowered types.
    /// Also, it prevents the type lowering code from recursing indefinitely
    /// on circular data types.
    typedef llvm::DenseMap<const Type*, const llvm::Type*> TypeMap;
    TypeMap loweredTypes;

    const DomainType *rewriteAbstractDecl(const AbstractDomainDecl *abstract);

    const llvm::IntegerType *getTypeForWidth(unsigned numBits);

    void addInstanceRewrites(const DomainInstanceDecl *instance);

    /// \brief Returns a reference to a slot in the type map corresponding to
    /// the given Comma type.
    ///
    /// If the type has not been previously lowered, the reference returned is
    /// to a null pointer.
    const llvm::Type *&getLoweredType(const Type *type) {
        return loweredTypes.FindAndConstruct(type).second;
    }
};

}; // end comma namespace

#endif
