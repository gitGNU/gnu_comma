The Comma Project: http://www.nongnu.org/comma

Overview
--------

This directory tree contains the source code for the Comma compiler, various
documents describing the system, as well as the project website.  The contents
of this distribution are licenced under the MIT license (also known as the Expat
license) -- see the file LICENSE.txt for details.

The Comma project consists of two main tasks:

   1) Writing a specification (the Commaspec) for the Comma programming
      language.

   2) Implementing a compiler conforming to the Commaspec.  This effort uses C++
      as the implementation language and interfaces with the LLVM system
      (http://llvm.org).

This project is in the planning/alpha stage.  The code and documentation, as it
stands, is not complete or functional.  If you are interested in developing the
system, the projects homepage provides links to the appropriate mailing list.


Building
--------

At this time, only developers should be building this software.  There is no
practical purpose for compiling the system unless one is interested in
testing or contributing to the project.

The system uses the traditional "configure and make" paradigm.

ONLY OUT OF TREE BUILDS ARE SUPPORTED.

Comma requires LLVM in order to build.  If the configure stage cannot detect
LLVM, you should tell configure where the llvm-config utility lives using the
--with-llvm switch.

The command sequence to build the system might resemble:

  > git clone git://git.savannah.nongnu.org/comma.git
    ...
  > mkdir build-comma && cd build-comma
  > ../comma/configure && make

All output from the compilation resides in the build directory.

Executable results are placed in a directory matching your hosts triplet
(e.g. x86_64-unknown-gnu-linux for a generic 64 bit linux machine). Currently,
only a simple "driver" program is built.  This program reads a single file given
on the command line (or from stdin if the supplied file name is "-"), parses and
type checks it.  A non-zero exit status is returned if an error was detected.

In order to build the Doxygen documentation, supply "--enable-doxygen" to
configure.  The html output is available under doc/doxygen/html.

If you want to work on the Commaspec (the document defining the Comma language),
you should install xsltproc, docbook, and supply configure with the
"--enable-commaspec" switch.  The html output is available under doc/commaspec.

A DejaGNU test suite is under developement.  You can run the tests with a "make
check".
