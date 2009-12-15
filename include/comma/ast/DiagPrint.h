//===-- ast/DiagPrint.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file extends the diag namespace with AST pretty printing
///  services for DiagnosticStreams.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DIAGPRINT_HDR_GUARD
#define COMMA_AST_SIAGPRINT_HDR_GUARD

#include "comma/basic/Diagnostic.h"

namespace comma {

class Expr;
class Type;
class Decl;

namespace diag {

class PrintType : public DiagnosticComponent {
    comma::Type *type;
public:
    PrintType(comma::Type *type) : type(type) { }
    void print(llvm::raw_ostream &stream) const;
};

class PrintDecl : public DiagnosticComponent {
    comma::Decl *decl;
public:
    PrintDecl(comma::Decl *decl) : decl(decl) { }
    void print(llvm::raw_ostream &stream) const;
};

} // end diag namespace

} // end comma namespace

#endif
