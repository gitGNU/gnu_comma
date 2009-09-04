
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
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/System/Host.h"
#include "llvm/System/Path.h"

#include <iostream>
#include <fstream>

using namespace comma;

// The input file name as a positional argument.  The default of "-" means read
// from standard input.
llvm::cl::opt<std::string>
InputFile(llvm::cl::Positional,
          llvm::cl::desc("<input file>"),
          llvm::cl::init("-"));

// A switch that asks the driver to perform syntatic and semantic processing
// only.
llvm::cl::opt<bool>
SyntaxOnly("fsyntax-only",
           llvm::cl::desc("Only perform syntatic and semantic analysis."));

int main(int argc, char **argv)
{
    Diagnostic diag;

    llvm::cl::ParseCommandLineOptions(argc, argv);

    llvm::sys::Path path(InputFile);
    TextProvider tp(path);
    IdentifierPool idPool;
    AstResource resource(tp, idPool);
    CompilationUnit cu(path);
    TypeCheck tc(diag, resource, &cu);
    Parser p(tp, idPool, tc, diag);
    bool status;

    while (p.parseTopLevelDeclaration());
    status = !p.parseSuccessful() || !tc.checkSuccessful();


    if (!status && !SyntaxOnly) {
        llvm::InitializeAllTargets();

        // FIXME: CodeGen should handle all of this.
        llvm::Module *M =
            new llvm::Module("test_module", llvm::getGlobalContext());
        std::string message;
        const llvm::Target *target;
        const llvm::TargetMachine *machine;
        const llvm::TargetData *data;

        std::string triple = llvm::sys::getHostTriple();
        M->setTargetTriple(triple);
        target = llvm::TargetRegistry::lookupTarget(triple, message);
        if (!target) {
            std::cerr << "Could not auto-select target architecture for "
                      << triple << ".\n   : "
                      << message << std::endl;
            return 1;
        }

        machine = target->createTargetMachine(triple, "");
        data = machine->getTargetData();
        M->setDataLayout(data->getStringRepresentation());

        CodeGen CG(M, *data, resource);

        typedef CompilationUnit::decl_iterator iterator;

        for (iterator iter = cu.beginDeclarations();
             iter != cu.endDeclarations(); ++iter) {
            CG.emitToplevelDecl(*iter);
        }

        M->print(llvm::outs(),0);
    }

    return status;
}
