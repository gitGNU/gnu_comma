
// This tool is temporary.  It drives the lexer, parser, and type checker over
// the contents of a single file.

#include "config.h"
#include "comma/ast/Ast.h"
#include "comma/ast/AstResource.h"
#include "comma/basic/TextProvider.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/codegen/Generator.h"
#include "comma/parser/Parser.h"
#include "comma/typecheck/Checker.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/System/Host.h"
#include "llvm/System/Path.h"
#include "llvm/System/Process.h"
#include "llvm/System/Program.h"
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

// Emit llvm IR in assembly form.
llvm::cl::opt<bool>
EmitLLVM("emit-llvm",
         llvm::cl::desc("Emit llvm assembly."));

// Emit llvm IR in bitcode form.
llvm::cl::opt<bool>
EmitLLVMBitcode("emit-llvm-bc",
                llvm::cl::desc("Emit llvm bitcode."));

// Output file.  Default to stdout.
llvm::cl::opt<std::string>
OutputFile("o",
           llvm::cl::desc("Defines the output file."),
           llvm::cl::init("-"));

// Disable optimizations.
llvm::cl::opt<bool>
DisableOpt("disable-opt",
           llvm::cl::desc("Disable all optimizations."));

namespace {

// Selects a procedure to use as an entry point and generates the corresponding
// main function.
int emitEntryPoint(Generator *Gen, const CompilationUnit &cu)
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

    Gen->emitEntry(proc);
    return 0;
}

// Emits llvm IR in either assembly or bitcode format to the given file.  If
// emitBitcode is true llvm bitcode is generated, otherwise assembly. Returns
// zero on sucess and one otherwise.
int outputIR(llvm::Module *M, llvm::sys::Path &outputPath, bool emitBitcode)
{

    // If the output is to be sent to stdout and bitcode was requested ensure
    // that stdout has been redirected to a file or a pipe.
    //
    // FIXME: LLVM provides CheckBitcodeOutputToConsole for this purpose.
    // Unfortunately in 2.6 detection is limited to llvm::outs() in particular
    // and will not recognize the raw_fd_ostream we create bellow.  2.6-svn
    // provides an improved version.  Upgrade when 2.7 is supported.
    if (outputPath.str() == "-" &&
        llvm::sys::Process::StandardOutIsDisplayed() &&
        emitBitcode) {
        llvm::errs() <<
            "Printing of bitcode is not supported.  Redirect the \n"
            "output to a file or pipe, or specify an output file \n"
            "with the `-o` option.\n";
        return 1;
    }

    // Determine the output file mode.  If an output file was given and we are
    // emitting bitcode use a binary stream.
    unsigned mode = (emitBitcode && !outputPath.isEmpty()) ?
        llvm::raw_fd_ostream::F_Binary : 0;

    // Open a stream to the output.
    std::string message;
    std::auto_ptr<llvm::raw_ostream> output(
        new llvm::raw_fd_ostream(outputPath.c_str(), message, mode));
    if (!message.empty()) {
        llvm::errs() << message << '\n';
        return 1;
    }

     if (emitBitcode)
         llvm::WriteBitcodeToFile(M, *output);
     else
        M->print(*output, 0);

     return 0;
}

int outputExec(llvm::Module *M)
{
    // Create a temporary file to hold bitcode output.  If we are reading from
    // stdin derive the temporaty file from a file name of `stdin'.
    llvm::sys::Path tmpBitcode;
    if (InputFile == "-")
        tmpBitcode = "stdin";
    else
        tmpBitcode = InputFile;
    tmpBitcode = tmpBitcode.getBasename();

    std::string message;
    tmpBitcode.createTemporaryFileOnDisk(false, &message);
    if (!message.empty()) {
        llvm::errs() << message << '\n';
        return 1;
    }

    // Determine the output file.  If no name was explicity given on the command
    // line derive the output file from the input file name, or use "a.out" if
    // we are reading from stdin.
    llvm::sys::Path outputPath;
    if (InputFile == "-" && OutputFile == "-")
        outputPath = "a.out";
    else if (OutputFile != "-")
        outputPath = OutputFile;
    else {
        outputPath = InputFile;
        assert(outputPath.getSuffix() == "cms" && "Not a .cms file!");
        outputPath = outputPath.getBasename();
    }

    // Generate the library path which contains the runtime library.
    std::string libflag = "-L" COMMA_BUILD_ROOT "/lib";

    // Locate llvm-ld.
    llvm::sys::Path llvm_ld = llvm::sys::Program::FindProgramByName("llvm-ld");
    if (llvm_ld.isEmpty()) {
        llvm::errs() << "Cannot locate `llvm-ld'.\n";
        return 1;
    }

    // Write bitcode out to the temporary file.
    if (outputIR(M, tmpBitcode, true) != 0) {
        tmpBitcode.eraseFromDisk(false, &message);
        if (!message.empty())
            llvm::errs() << message << '\n';
        return 1;
    }

    // Build the argument vector for llvm-ld and execute.
    llvm::SmallVector<const char*, 16> args;
    args.push_back(llvm_ld.c_str());
    args.push_back("-native");
    args.push_back("-o");
    args.push_back(outputPath.c_str());
    args.push_back(libflag.c_str());
    args.push_back("-lruntime");
    args.push_back(tmpBitcode.c_str());

    // Disable optimizations if requested.
    if (DisableOpt)
        args.push_back("-disable-opt");

    // Terminate the argument list.
    args.push_back(0);

    bool status =
        llvm::sys::Program::ExecuteAndWait(llvm_ld, &args[0], 0, 0, 0, 0, &message);

    tmpBitcode.eraseFromDisk(false, &message);
    if (!message.empty()) {
        llvm::errs() << message << '\n';
        return 1;
    }
    return status;
}

} // end anonymous namespace.

int main(int argc, char **argv)
{
    Diagnostic diag;

    llvm::cl::ParseCommandLineOptions(argc, argv);

    llvm::sys::Path path(InputFile);
    if (!path.canRead() && path.str() != "-") {
        llvm::errs() << "Cannot open `" << path.str() <<"' for reading.\n";
        return 1;
    }
    if (path.getSuffix() != "cms" && path.str() != "-") {
        llvm::errs() << "Input files must have a `.cms' extension.\n";
        return 1;
    }

    TextProvider tp(path);
    IdentifierPool idPool;
    AstResource resource(tp, idPool);
    CompilationUnit cu(path);
    std::auto_ptr<Checker> check(Checker::create(diag, resource, &cu));
    Parser p(tp, idPool, *check, diag);

    // Drive the parser and type checker.
    while (p.parseTopLevelDeclaration());

    if (diag.numErrors() != 0)
        return 1;

    if (SyntaxOnly)
        return 0;

    llvm::InitializeAllTargets();

    // FIXME: CodeGen should handle all of this.
    llvm::LLVMContext context;
    llvm::Module *M = new llvm::Module("test_module", context);
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

    std::auto_ptr<Generator> Gen(Generator::create(M, *data, resource));

    typedef CompilationUnit::decl_iterator iterator;

    for (iterator iter = cu.beginDeclarations();
         iter != cu.endDeclarations(); ++iter) {
        Gen->emitToplevelDecl(*iter);
    }

    // Generate an entry point and native executable if requested.
    if (!EntryPoint.empty()) {
        if (emitEntryPoint(Gen.get(), cu))
            return 1;
        if (!(EmitLLVM || EmitLLVMBitcode))
            return outputExec(M);
    }

    // Emit assembly or bitcode.
    if (EmitLLVM || EmitLLVMBitcode) {
        if (EmitLLVM && EmitLLVMBitcode) {
            llvm::errs() <<
                "Cannot specify both `-emit-llvm' and `-emit-llvm-bc'.\n";
            return 1;
        }
        llvm::sys::Path outFile(OutputFile);
        bool emitBitcode = EmitLLVMBitcode ? true : false;
        return outputIR(M, outFile, emitBitcode);
    }

    // There were no output options given.
    return 0;
}
