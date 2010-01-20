//===-- basic/PrimitiveOps.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/IdentifierInfo.h"
#include "comma/basic/PrimitiveOps.h"

using namespace comma::primitive_ops;

bool comma::primitive_ops::denotesBinaryOp(const llvm::StringRef &string)
{
    const char* name = string.data();
    size_t length = string.size();

    if (length == 1) {
        switch (*name) {
        default:
            return false;
        case '=':
        case '+':
        case '*':
        case '-':
        case '>':
        case '<':
        case '/':
            return true;
        }
    }
    else if (length == 2) {
        switch (*name) {
        default:
            return false;
        case '/':
        case '<':
        case '>':
            return name[1] == '=';

        case '*':
            return name[1] == '*';

        case 'o':
            return name[1] == 'r';
        }
    }
    else if (length == 3) {
        return (string.compare("and") == 0 ||
                string.compare("not") == 0 ||
                string.compare("xor") == 0 ||
                string.compare("mod") == 0 ||
                string.compare("rem") == 0);
    }
    return false;
}

bool comma::primitive_ops::denotesUnaryOp(const llvm::StringRef &string)
{
    const char *name = string.data();
    size_t length = string.size();

    if (length == 1) {
        switch (*name) {
        default:
            return false;
        case '+':
        case '-':
            return true;
        }
    }
    else
        return string.compare("not") == 0;
}

bool comma::primitive_ops::denotesOperator(const llvm::StringRef &string)
{
    return denotesBinaryOp(string) || denotesUnaryOp(string);
}

bool
comma::primitive_ops::denotesBinaryOp(const comma::IdentifierInfo *idInfo)
{
    return denotesBinaryOp(idInfo->getString());
}

bool
comma::primitive_ops::denotesUnaryOp(const comma::IdentifierInfo *idInfo)
{
    return denotesUnaryOp(idInfo->getString());
}

bool
comma::primitive_ops::denotesOperator(const comma::IdentifierInfo *idInfo)
{
    llvm::StringRef name(idInfo->getString());
    return denotesBinaryOp(name) || denotesUnaryOp(name);
}

