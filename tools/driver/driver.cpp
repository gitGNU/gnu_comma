
// This tool is temporary.  It drives the lexer, parser, and type checker over
// the contents of a single file.

#include "comma/ast/Ast.h"
#include "comma/ast/AstResource.h"
#include "comma/basic/TextProvider.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/codegen/CodeGen.h"
#include "comma/parser/Parser.h"
#include "comma/typecheck/Checker.h"

#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/System/Host.h"
#include "llvm/System/Path.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Target/TargetSelect.h"

#include <fstream>
#include <iostream>
#include <memory>

using namespace comma;
using llvm::dyn_cast;

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

// The entry switch defining the main function to call.
llvm::cl::opt<std::string>
EntryPoint("e",
           llvm::cl::desc("Comma procedure to use as an entry point."));


static int emitEntryPoint(CodeGen &CG, const CompilationUnit &cu)
{
    // We must have a well formed entry point string of the form "D.P", where D
    // is the context domain and P is the procedure to call.
    typedef std::string::size_type Index;
    Index dotPos = EntryPoint.find('.');

    if (dotPos == std::string::npos) {
        llvm::errs() << "Malformed entry designator `" << EntryPoint << "'\n";
        return 1;
    }

    std::string domainName = EntryPoint.substr(0, dotPos);
    std::string procName = EntryPoint.substr(dotPos + 1);

    // Find a declaration in the given compilation unit which matches the needed
    // domain.
    DomainDecl *context = 0;
    typedef CompilationUnit::decl_iterator ctx_iterator;
    for (ctx_iterator I = cu.beginDeclarations();
         I != cu.endDeclarations(); ++I) {
        if (!(context = dyn_cast<DomainDecl>(*I)))
            continue;
        if (domainName == context->getString())
            break;
    }

    if (!context) {
        llvm::errs() << "Entry domain `" << domainName << "' not found.\n";
        return 1;
    }

    // Find a nullary procedure declaration with the given name.
    ProcedureDecl *proc = 0;
    DomainInstanceDecl *instance = context->getInstance();
    typedef DeclRegion::DeclIter decl_iterator;
    for (decl_iterator I = instance->beginDecls();
         I != instance->endDecls(); ++I) {
        ProcedureDecl *candidate = dyn_cast<ProcedureDecl>(*I);
        if (!candidate)
            continue;
        if (procName != candidate->getString())
            continue;
        if (candidate->getArity() == 0) {
            proc = candidate;
            break;
        }
    }

    if (!proc) {
        llvm::errs() << "Entry procedure `" << procName << "' not found.\n";
        return 1;
    }

    CG.emitEntry(proc);
    return 0;
}

int main(int argc, char **argv)
{
    Diagnostic diag;

    llvm::cl::ParseCommandLineOptions(argc, argv);

    llvm::sys::Path path(InputFile);
    TextProvider tp(path);
    IdentifierPool idPool;
    AstResource resource(tp, idPool);
    CompilationUnit cu(path);
    std::auto_ptr<Checker> check(Checker::create(diag, resource, &cu));
    Parser p(tp, idPool, *check, diag);
    bool status;

    while (p.parseTopLevelDeclaration());
    status = !p.parseSuccessful() || !check->checkSuccessful();


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

        // If an entry point was requested.  Emit it.
        if (!EntryPoint.empty())
            if (emitEntryPoint(CG, cu))
                return 1;

        M->print(llvm::outs(),0);
    }

    return status;
}
