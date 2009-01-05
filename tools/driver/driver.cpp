
// This tool is temporary.  It drives the lexer, parser, and type checker over
// the contents of a single file.

#include "comma/basic/TextProvider.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/parser/Parser.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Ast.h"
#include "llvm/System/Path.h"
#include <iostream>
#include <fstream>

using namespace comma;

int main(int argc, const char **argv)
{
    Diagnostic diag;

    if (argc != 2) {
        std::cerr << "Usage : " << argv[0] << " <filename>"
                  << std::endl;
        return 1;
    }
    llvm::sys::Path path(argv[1]);
    TextProvider tp(path);
    CompilationUnit cu(path);
    TypeCheck tc(diag, tp, &cu);
    Parser p(tp, tc, diag);

    while(p.parseTopLevelDeclaration());

    return 0;
}

IdentifierPool IdentifierPool::spool;
