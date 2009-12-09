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

This program reads a single file given on the command line (or from stdin if the
supplied file name is "-" or missing), parses, type checks, and then emits
either LLVM assembly code, LLVM bitcode, or a native unoptimized executable.

There are only a few command line options:

 -o <file>: Specifies the output file the driver should write to.  If not given
  or specified as "-" then stdout is written to.

 -fsyntax-only: Input is parsed and type checked but codegen is skipped.  This
  is used to check the parser and type checker in isolation.  The only output
  produced is diagnostic messages.

 -emit-llvm: Parse, type check, and codegen the input.  Produces LLVM assembly
  as output.

 -emit-llvm-bc: Like -emit-llvm but produces an LLVM bitcode file instead.  The
  driver will not print bitcode directly to the terminal.  If stdout is to be
  used it must be redirected to a file or pipe.

 -e: Define an entry point.  This tells the driver to emit a main stub function
  which calls into the generated Comma code, which in turn allows the resulting
  LLVM IR to be linked into an executable.  Currently, an entry point must be
  the name of a nullary subroutine exported by a non-generic domain.  For
  example, given:

     domain D with
        procedure P;
     add
        procedure P is
           ...
        end P;
     end D;

  then D.P satisfies the requirements of an entry point.  For example, to print
  out the LLVM assembly including the sub function the corresponding invocation
  of the driver program would be:

     > driver -e D.P foo.cms -emit-llvm

  When neither -emit-llvm or -emit-llvm-bc are given then the driver will
  generate a native executable.  In this case the -o flag can be used specify
  the name of the executable.  Otherwise, the executable is named after the
  given input file, or "a.out" when reading from stdin.  Examples:

     > cat foo.cms | driver -e D.P   # produces executable "a.out"

     > driver foo.cms -e D.P         # produces executable "foo"

     > driver foo.cms -e D.P -o bar  # produces executable "bar"

