//===-- ast/Ast.cpp ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "AstDumper.h"
#include "comma/ast/Ast.h"
#include "comma/ast/SubroutineRef.h"

#include <algorithm>

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

const char *Ast::kindStrings[LAST_AstKind] = {
    "PackageDecl",
    "BodyDecl",

    "AccessDecl",
    "EnumerationDecl",
    "IncompleteTypeDecl",
    "IntegerDecl",
    "ArrayDecl",
    "PrivateTypeDecl",
    "RecordDecl",

    "PkgInstanceDecl",

    "LoopDecl",
    "ObjectDecl",
    "ParamValueDecl",
    "RenamedObjectDecl",

    "ProcedureDecl",
    "FunctionDecl",
    "EnumLiteral",
    "PosAD",
    "ValAD",
    "UseDecl",
    "ExceptionDecl",
    "ComponentDecl",

    "UniversalType",
    "FunctionType",
    "ProcedureType",

    "AccessType",
    "ArrayType",
    "EnumerationType",
    "IncompleteType",
    "IntegerType",
    "PrivateType",
    "RecordType",

    "AllocatorExpr",
    "ConversionExpr",
    "DiamondExpr",
    "DeclRefExpr",
    "DereferenceExpr",
    "FunctionCallExpr",
    "IndexedArrayExpr",
    "IntegerLiteral",
    "NullExpr",
    "AggregateExpr",
    "QualifiedExpr",
    "SelectedExpr",
    "StringLiteral",

    "FirstAE",
    "FirstArrayAE",
    "LastArrayAE",
    "LengthAE",
    "LastAE",

    "AssignmentStmt",
    "BlockStmt",
    "ForStmt",
    "HandlerStmt",
    "IfStmt",
    "LoopStmt",
    "ExitStmt",
    "NullStmt",
    "ProcedureCallStmt",
    "RaiseStmt",
    "ReturnStmt",
    "StmtSequence",
    "WhileStmt",
    "PragmaStmt",

    "KeywordSelector",
    "DSTDefinition",
    "STIndication",
    "Range",
    "ArrayRangeAttrib",
    "ScalarRangeAttrib",
    "SubroutineRef",
    "TypeRef",
    "ExceptionRef",
    "Identifier",
    "ComponentKey",
    "PrivatePart"
};

void Ast::dump()
{
    AstDumper dumper(llvm::errs());
    dumper.dump(this);
    llvm::errs().flush();
}

//===----------------------------------------------------------------------===//
// Nodes which do not belong to any of the major branches in the AST hierarchy
// (Type, Decl, Expr) have their out or line members define below.

//===----------------------------------------------------------------------===//
// SubroutineRef

void SubroutineRef::verify()
{
    assert(!decls.empty() && "Empty SubroutineRef!");
    SubroutineDecl *elem = decls[0];
    IdentifierInfo *name = elem->getIdInfo();
    bool isaFunction = isa<FunctionDecl>(elem);
    unsigned numDecls = numDeclarations();

    for (unsigned i = 1; i < numDecls; ++i) {
        SubroutineDecl *cursor = decls[i];
        verify(cursor, name, isaFunction);
    }
}

void SubroutineRef::verify(SubroutineDecl *decl,
                           IdentifierInfo *name, bool isaFunction)
{
    assert(decl->getIdInfo() == name &&
           "All declarations must have the same identifier!");
    if (isaFunction)
        assert(isa<FunctionDecl>(decl) && "Declaration type mismatch!");
    else
        assert(isa<ProcedureDecl>(decl) && "Declaration type mismatch!");
}

void SubroutineRef::addDeclaration(SubroutineDecl *srDecl)
{
    if (decls.empty())
        decls.push_back(srDecl);
    else {
        IdentifierInfo *name = getIdInfo();
        bool isaFunction = referencesFunctions();
        verify(srDecl, name, isaFunction);
        decls.push_back(srDecl);
    }
}

bool SubroutineRef::contains(const SubroutineDecl *srDecl) const
{
    const_iterator I = std::find(begin(), end(), srDecl);
    if (I != end())
        return true;
    return false;
}

bool SubroutineRef::contains(const SubroutineType *srType) const
{
    for (const_iterator I = begin(); I != end(); ++I) {
        SubroutineDecl *target = *I;
        if (target->getType() == srType)
            return true;
    }
    return false;
}

bool SubroutineRef::keepSubroutinesWithArity(unsigned arity)
{
    DeclVector::iterator I = decls.begin();
    while (I != decls.end()) {
        SubroutineDecl *decl = *I;
        if (decl->getArity() != arity)
            I = decls.erase(I);
        else
            ++I;
    }
    return !decls.empty();
}
