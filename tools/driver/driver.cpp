
// This tool is temporary.  It drives the lexer, parser, and type checker over
// the contents of a single file.

#include "comma/basic/TextProvider.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/parser/Parser.h"
#include "comma/typecheck/TypeCheck.h"
#include "comma/ast/Ast.h"
#include "comma/ast/AstResource.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenRoutine.h"

#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetMachineRegistry.h"
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
    bool status;

    while (p.parseTopLevelDeclaration());
    status = !p.parseSuccessful() || !tc.checkSuccessful();

    if (!status) {

        llvm::Module M("test_module");
        std::string message;
        const llvm::TargetMachineRegistry::entry *arch;
        llvm::TargetMachine *target;
        const llvm::TargetData *data;

        // FIXME: There are several ways to get the target triple.  We could try
        // and derive it directly, use the result of config.guess, or call
        // llvm::sys::getHostTriple.  The latter option is probably the best
        // path, but requires recent 2.6svn.
        M.setTargetTriple("x86_64-unknown-linux-gnu");

        arch = llvm::TargetMachineRegistry::getClosestStaticTargetForModule(M, message);
        if (!arch) {
            std::cerr << "Could not auto-select target architecture.\n   : "
                      << message << std::endl;
            return 1;
        }

        target = arch->CtorFn(M, "");
        data   = target->getTargetData();
        M.setDataLayout(data->getStringRepresentation());

        CodeGen CG(&M, *data);

        typedef CompilationUnit::decl_iterator iterator;

        for (iterator iter = cu.beginDeclarations();
             iter != cu.endDeclarations(); ++iter) {
            CG.emitToplevelDecl(*iter);
        }

        M.print(std::cout, 0);
    }

    return status;
}
