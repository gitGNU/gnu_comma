
// This tool is temporary.  It drives the lexer, parser, and type checker over
// the contents of a single file.

#include "comma/basic/TextProvider.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/parser/Parser.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Ast.h"
#include "comma/ast/AstResource.h"
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
    IdentifierPool idPool;
    TextProvider tp(path);
    AstResource resource(tp, idPool);
    CompilationUnit cu(path);
    TypeCheck tc(diag, resource, &cu);
    Parser p(tp, idPool, tc, diag);

    while(p.parseTopLevelDeclaration());
    return 0;
}
