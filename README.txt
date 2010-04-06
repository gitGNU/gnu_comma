The Comma Project: http://www.nongnu.org/comma

===--------------------------------------------------------------------------===
-- Overview

This directory tree contains the source code for the Comma compiler, various
documents describing the system, as well as the project website.  The majority
of this distribution is licensed under the MIT license (also known as the Expat
license) -- see the file LICENSE.txt for details.  Components of this
distribution which are licensed differently reside in their own directory with
corresponding README.txt and LICENSE.txt files.  All components of this
distribution are provided under free software licenses.

The Comma project consists of two main tasks:

   1) Writing a specification (the Commaspec) for the Comma programming
      language.

   2) Implementing a compiler conforming to the Commaspec.  This effort uses C++
      as the implementation language and interfaces with the LLVM system
      (http://llvm.org).

This project is at an experimental stage.  The compiler is far from complete but
is capable of translating fairly non-trivial programs.  Documentation is still
lacking.  If you are interested in developing the system or have any questions
feel free to send a message to the comma-devel mailing list.  Links to the list
are accessible from www.nongnu.org/comma.

===--------------------------------------------------------------------------===
-- Building

At this time, only developers should be building this software.  There is no
practical purpose for compiling the system unless one is interested in
testing or contributing to the project.

The system uses the traditional "configure and make" paradigm.

ONLY OUT-OF-TREE BUILDS ARE SUPPORTED.

Comma requires LLVM in order to build.  If the configure stage cannot detect
LLVM, you should tell configure where the llvm-config utility lives using the
--with-llvm switch.

The command sequence to build the system might resemble:

   > git clone git://git.savannah.nongnu.org/comma.git
     ...
   > mkdir build-comma && cd build-comma
   > ../comma/configure && make

All output from the compilation resides in the build directory.  Executable
results are placed in a directory matching your hosts triplet
(e.g. x86_64-unknown-gnu-linux for a generic 64 bit GNU/Linux machine).

In order to build the Doxygen documentation, supply "--enable-doxygen" to
configure.  The html output is available under doc/doxygen/html.

If you want to work on the Commaspec (the document defining the Comma language),
you should install xsltproc, docbook, and supply configure with the
"--enable-commaspec" switch.  The html output is available under doc/commaspec.

A DejaGNU test suite is under development.  You can run the tests with a "make
check".

===--------------------------------------------------------------------------===
-- The Driver Program

Currently, only a very simple "driver" program is built.  This is a temporary
tool that will be rewritten into a full-featured compiler driver in the future.

This program reads a single file given on the command line, parses, type checks,
and then emits either LLVM assembly code, LLVM bitcode, or a native executable.

If the input file depends on other program components (via a 'with' clause)
those components must be available in the directory the driver was invoked
from.  Moreover, the file names and the principle component they define must
match.  So, for example, a domain "D" should be defined in a file named "D.cms",
which in turn can be referred to via a with clause of the form "with D;".

There are only a few command line options:

 -o <file>: When generating a native executable, specifies the name of the
  executable.  If an output file name is not specified, it is taken to be the
  base name of the input file.

 -fsyntax-only: Input is parsed and type checked but codegen is skipped.  This
  is used to check the parser and type checker in isolation.  The only output
  produced is diagnostic messages.

 -emit-llvm: Parse, type check, and codegen the input.  Produces LLVM assembly
  as output for the input file name and any of its dependents.

 -emit-llvm-bc: Like -emit-llvm but produces an LLVM bitcode files instead.

 -e: Define an entry point and generate a native executable.  This tells the
  driver to emit a main stub function which calls into the generated Comma code,
  which in turn allows the resulting LLVM IR to be linked into an executable.
  Currently, an entry point must be the name of a nullary subroutine exported by
  a non-generic domain.  For example, given:

     domain D with
        procedure P;
     add
        procedure P is
           ...
        end P;
     end D;

  then D.P satisfies the requirements of an entry point.

 -d: Specifies the output directory.  If an --emit-llvm, --emit-llvm-bc, or -e
  flag is present, llvm IR (".ll" files) or llvm bitcode (".bc" files) are
  generated for the input file and all of its dependents.  The -d flag can be
  used to specify the directory where these output files should be written.

Here are a few examples of how to use the driver program:

  - "driver Foo.cms":  Parse, type check, and codegen Foo.cms (and all
    dependents) but do not produce any output.  The compilation will either
    succeed or fail.  This is useful for testing general system sanity.

  - "driver Foo.cms -e Foo.Run": Generate a native executable named Foo and a
    set of bitcode files in the current directory using the entry procedure
    Foo.Run.

  - "driver Foo.cms -e Foo.Run -d output -o result": Generate a native
    executable named 'result' and a set of bitcode files in the directory
    'output' using the entry procedure Foo.Run.

  - "driver Foo.cms -d output --emit-llvm":  Generate a set of llvm IR files in
    the directory 'output'.

