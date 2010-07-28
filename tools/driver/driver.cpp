
// This tool is temporary.  It drives the lexer, parser, and type checker over
// the contents of a single file.

#include "config.h"
#include "SourceManager.h"
#include "comma/ast/Ast.h"
#include "comma/ast/AstResource.h"
#include "comma/basic/TextManager.h"
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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>


using namespace comma::driver;
using llvm::dyn_cast;

// The input file name as a positional argument.
llvm::cl::opt<std::string>
InputFile(llvm::cl::Positional,
          llvm::cl::desc("<input file>"),
          llvm::cl::init(""));

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

// Output file.
llvm::cl::opt<std::string>
OutputFile("o",
           llvm::cl::desc("Defines the output file."),
           llvm::cl::init(""));

// Destination directory.  Default to pwd.
llvm::cl::opt<std::string>
DestDir("d",
        llvm::cl::desc("Destination directory."),
        llvm::cl::init(""));

// Disable optimizations.
llvm::cl::opt<bool>
DisableOpt("disable-opt",
           llvm::cl::desc("Disable all optimizations."));

// Dump the AST.
llvm::cl::opt<bool>
DumpAST("dump-ast",
        llvm::cl::desc("Dump ast to stderr."));

namespace {

// Selects a procedure to use as an entry point and generates the corresponding
// main function.
bool emitEntryPoint(Generator *Gen, const CompilationUnit &cu)
{
    // We must have a well formed entry point string of the form "D.P", where D
    // is the context domain and P is the procedure to call.
    typedef std::string::size_type Index;
    Index dotPos = EntryPoint.find('.');

    if (dotPos == std::string::npos) {
        llvm::errs() << "Malformed entry designator `" << EntryPoint << "'\n";
        return false;
    }

    std::string capsuleName = EntryPoint.substr(0, dotPos);
    std::string procName = EntryPoint.substr(dotPos + 1);

    // Bring the capsule and procedure name into canonical lower case form.
    std::transform(capsuleName.begin(), capsuleName.end(), capsuleName.begin(),
                   static_cast<int(*)(int)>(std::tolower));
    std::transform(procName.begin(), procName.end(), procName.begin(),
                   static_cast<int(*)(int)>(std::tolower));

    // Find a declaration in the given compilation unit which matches the needed
    // capsule.
    DeclRegion *region = 0;
    typedef CompilationUnit::decl_iterator ctx_iterator;
    for (ctx_iterator I = cu.begin_declarations();
         I != cu.end_declarations(); ++I) {
        if (PackageDecl *package = dyn_cast<PackageDecl>(*I)) {
            if (capsuleName == package->getString()) {
                region = package->getInstance();
                break;
            }
        }
    }

    if (!region) {
        llvm::errs() << "Entry capsule `" << capsuleName << "' not found.\n";
        return false;
    }

    // Find a nullary procedure declaration with the given name.
    ProcedureDecl *proc = 0;
    typedef DeclRegion::DeclIter decl_iterator;
    for (decl_iterator I = region->beginDecls();
         I != region->endDecls(); ++I) {
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
        return false;
    }

    Gen->emitEntry(proc);
    return true;
}

// Emits llvm IR in either assembly or bitcode format to the given file.  If
// emitBitcode is true llvm bitcode is generated, otherwise assembly. Returns
// zero on sucess and one otherwise.
bool outputIR(llvm::Module *M, llvm::sys::Path &outputPath, bool emitBitcode)
{
    // Ensure that the output path is not "-".  We never output to stdout.
    assert(outputPath.str() != "-" && "Cannout output to stdout!");

    // Determine the output file mode.  If we are emitting bitcode use a binary
    // stream.
    unsigned mode = emitBitcode ? llvm::raw_fd_ostream::F_Binary : 0;

    // Open a stream to the output.
    std::string message;
    std::auto_ptr<llvm::raw_ostream> output(
        new llvm::raw_fd_ostream(outputPath.c_str(), message, mode));
    if (!message.empty()) {
        llvm::errs() << message << '\n';
        return false;
    }

     if (emitBitcode)
         llvm::WriteBitcodeToFile(M, *output);
     else
         M->print(*output, 0);

     return true;
}

bool outputExec(std::vector<SourceItem*> &Items)
{
    // Determine the output file.  If no name was explicity given on the command
    // line derive the output file from the input file name.
    llvm::sys::Path outputPath;
    if (OutputFile.empty()) {
        llvm::sys::Path inputPath(InputFile);
        outputPath = DestDir;
        outputPath.appendComponent(inputPath.getBasename());
    }
    else
        outputPath = OutputFile;

    // Locate llvm-ld.
    llvm::sys::Path llvm_ld = llvm::sys::Program::FindProgramByName("llvm-ld");
    if (llvm_ld.isEmpty()) {
        llvm::errs() << "Cannot locate `llvm-ld'.\n";
        return false;
    }

    // Generate the library path which contains the runtime library.
    std::string libflag = "-L" COMMA_BUILD_ROOT "/lib";

    // Build the argument vector for llvm-ld and execute.
    llvm::SmallVector<const char*, 16> args;
    args.push_back(llvm_ld.c_str());
    args.push_back("-native");
    args.push_back("-o");
    args.push_back(outputPath.c_str());
    args.push_back(libflag.c_str());
    args.push_back("-lruntime");

    // Disable optimizations if requested.
    if (DisableOpt)
        args.push_back("-disable-opt");

    // Append all of the bitcode file names.
    for (unsigned i = 0; i < Items.size(); ++i)
        args.push_back(Items[i]->getBitcodePath().c_str());

    // Terminate the argument list.
    args.push_back(0);

    std::string message;
    if (llvm::sys::Program::ExecuteAndWait(
            llvm_ld, &args[0], 0, 0, 0, 0, &message)) {
        llvm::errs() << "Could not generate executable: " << message << '\n';
        return false;
    }
    return true;
}

bool generateSourceItem(SourceItem *Item, TextManager &Manager,
                        AstResource &Resource, Diagnostic &Diag,
                        bool EmitEntry = false)
{
    llvm::InitializeAllTargets();

    // FIXME: CodeGen should handle all of this.
    llvm::LLVMContext context;
    std::string message;
    const llvm::Target *target;
    const llvm::TargetMachine *machine;
    const llvm::TargetData *data;

    std::auto_ptr<llvm::Module> M(
        new llvm::Module(Item->getSourcePath().str(), context));

    std::string triple = llvm::sys::getHostTriple();
    M->setTargetTriple(triple);
    target = llvm::TargetRegistry::lookupTarget(triple, message);
    if (!target) {
        std::cerr << "Could not auto-select target architecture for "
                  << triple << ".\n   : "
                  << message << std::endl;
        return false;
    }

    machine = target->createTargetMachine(triple, "");
    data = machine->getTargetData();
    M->setDataLayout(data->getStringRepresentation());

    std::auto_ptr<Generator> Gen(
        Generator::create(M.get(), *data, Manager, Resource));

    CompilationUnit *CU = Item->getCompilation();
    Gen->emitCompilationUnit(CU);

    // Generate an entry point and native executable if requested.
    if (EmitEntry && !EntryPoint.empty()) {
        if (!emitEntryPoint(Gen.get(), *CU))
            return false;
    }

    // Emit llvm IR if requested.
    if (EmitLLVM) {
        llvm::sys::Path output(DestDir);

        if (!output.exists()) {
            std::string message;
            if (output.createDirectoryOnDisk(true, &message)) {
                llvm::errs() << "Could not create output directory: "
                             << message << '\n';
                return false;
            }
        }

        output.appendComponent(Item->getSourcePath().getBasename());
        output.appendSuffix("ll");
        if (!outputIR(M.get(), output, false))
            return false;
        Item->setIRPath(output);
    }

    // Emit llvm bitcode if requested.
    if (EmitLLVMBitcode) {
        llvm::sys::Path output(DestDir);

        if (!output.exists()) {
            std::string message;
            if (output.createDirectoryOnDisk(true, &message)) {
                llvm::errs() << "Could not create output directory: "
                             << message << '\n';
                return false;
            }
        }

        output.appendComponent(Item->getSourcePath().getBasename());
        output.appendSuffix("bc");
        if (!outputIR(M.get(), output, true))
            return false;
        Item->setBitcodePath(output);
    }

    return true;
}

Decl *findPrincipleDeclaration(Diagnostic &Diag, SourceItem *Item)
{
    CompilationUnit *CU = Item->getCompilation();
    assert(CU && "Dependency not resolved!");

    std::string target = Item->getSourcePath().getBasename();
    std::transform(target.begin(), target.end(), target.begin(), tolower);

    typedef CompilationUnit::decl_iterator iterator;
    iterator I = CU->begin_declarations();
    iterator E = CU->end_declarations();
    for ( ; I != E; ++I) {
        Decl *decl = *I;
        std::string candidate = decl->getIdInfo()->getString();
        std::transform(candidate.begin(), candidate.end(), candidate.begin(),
                       tolower);
        if (candidate == target)
            return decl;
    }

    Diag.getStream() << "File `" << Item->getSourcePath().str()
                     << "' does not define a unit named `"
                     << target << "'\n";
    return 0;
}

bool compileSourceItem(SourceItem *RootItem, AstResource &Resource,
                       Diagnostic &Diag)
{
    typedef std::vector<SourceItem*>::reverse_iterator source_iterator;
    std::vector<SourceItem*> Items;

    RootItem->extractDependencies(Items);

    // If an entry point was defined we must generate an executable.  Ensure
    // bitcode is generated in this case.
    if (!EntryPoint.empty())
        EmitLLVMBitcode = true;

    // Process the dependencies in reverse order.
    TextManager TM;
    for (source_iterator I = Items.rbegin(), E = Items.rend(); I != E; ++I) {
        SourceItem *Item = *I;
        TextProvider &TP = TM.create(Item->getSourcePath());
        std::auto_ptr<CompilationUnit> CU(
            new CompilationUnit(Item->getSourcePath()));
        std::auto_ptr<Checker> TC(
            Checker::create(TM, Diag, Resource, CU.get()));

        Parser P(TP, Resource.getIdentifierPool(), *TC, Diag);

        // Initialize the compilation unit with any needed dependencies.
        for (SourceItem::iterator D = Item->begin(); D != Item->end(); ++D) {
            // FIXME: We allow multiple declarations in compilation units for
            // now (so that the test suite does not break).  In the future we
            // will probably insist that a source item provides only one unit
            // with the required name.  For now, ensure that at least one unit
            // exists with the required name (all other declarations are
            // effectively private within the source item).
            Decl *principle;
            if (!(principle = findPrincipleDeclaration(Diag, *D)))
                return false;
            CU->addDependency(principle);
        }

        // Drive the parser and type checker.
        P.parseCompilationUnit();

        // We are finished with the source code.  Close the TextProvider and
        // release the associated resources.
        TP.close();

        // Dump the ast if we were asked.
        if (DumpAST) {
            typedef CompilationUnit::decl_iterator iterator;
            iterator I = CU->begin_declarations();
            iterator E = CU->end_declarations();
            for ( ; I != E; ++I) {
                Decl *decl = *I;
                decl->dump();
                llvm::errs() << '\n';
            }
        }

        // Stop processing if the SourceItem did not parse/typecheck cleanly.
        if (Diag.numErrors() != 0)
            return false;

        // Add the compilation unit to the SourceItem.
        Item->setCompilation(CU.release());

        // Codegen if needed.
        if (!SyntaxOnly) {
            bool emitEntry = Item == RootItem;
            if (!generateSourceItem(Item, TM, Resource, Diag, emitEntry))
                return false;
        }
    }

    // FIXME: We might want to insist that the root source item declares a unit
    // with a matching name.  However, such a constraint would break the test
    // suite ATM since the current convention is to use an entry point named
    // Test.Run regardless of the file name (or perhaps we should not care,
    // since the dependency graph is cycle free nothing can refer to the root
    // item anyway).

    // If an entry point was defined, generate a native executable.
    if (!EntryPoint.empty())
        return outputExec(Items);

    return true;
}

} // end anonymous namespace

using namespace comma::driver;

int main(int argc, char **argv)
{
    llvm::cl::ParseCommandLineOptions(argc, argv);

    if (InputFile.empty()) {
        llvm::errs() << "Missing input file.\n";
        return 1;
    }

    llvm::sys::Path path(InputFile);
    if (!path.canRead()) {
        llvm::errs() << "Cannot open `" << path.str() <<"' for reading.\n";
        return 1;
    }
    if (path.getSuffix() != "cms") {
        llvm::errs() << "Input files must have a `.cms' extension.\n";
        return 1;
    }

    if (DestDir.empty())
        DestDir = llvm::sys::Path::GetCurrentDirectory().str();

    Diagnostic diag;
    IdentifierPool idPool;
    AstResource resource(idPool);
    SourceManager SM(idPool, diag);
    SourceItem *Item = SM.getSourceItem(path);

    if (Item)
        return compileSourceItem(Item, resource, diag) ? 0 : 1;
    else
        return 1;
}
