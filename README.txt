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
about the system feel free to send a message to the comma-devel mailing list.
Links to the list are accessible from www.nongnu.org/comma.

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

Currently, only a very simple "driver" program is built.  This program reads a
single file given on the command line (or from stdin if the supplied file name
is "-" or missing), parses, type checks it, and then emits LLVM assembly code to
stdout if there were no errors.  A non-zero exit status is returned if an error
was detected.

If the -fsyntax-only flag is passed to the driver the input is parsed and
type checked but codegen is skipped.

An entry procedure can be specified via the -e flag.  This tells the driver to
emit a main stub function which calls into the generated Comma code, which in
turn allows the resulting LLVM IR to be optimized, assembled and linked into an
executable.  Currently, an entry point must be the name of a nullary subroutine
exported by a non-generic domain.  For example, given:

   domain D with
      procedure P;
   add
      procedure P is
         ...
      end P;
   end D;

then D.P satisfies the requirements of an entry point.  The corresponding
invocation of the driver program would be:

   > driver -e D.P D.cms
