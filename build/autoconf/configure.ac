dnl **-- configure.ac --********************************************************
dnl *
dnl * This file is distributed under the MIT License.  See LICENSE.txt for
dnl * details.
dnl *
dnl * Copyright (C) 2008, Stephen Wilson
dnl *
dnl ****************************************************************************


dnl ****************************************************************************
dnl * Initialize
dnl ****************************************************************************

AC_INIT([[Comma]],[[0.07]],[wilsons@start.ca])

dnl Ensure we are using a recent enough version of Autoconf.
AC_PREREQ(2.60)

dnl Verify that the source directory is valid.
AC_CONFIG_SRCDIR(["Makefile.common.in"])

dnl Advertise where our extra Autoconf macros live.
AC_CONFIG_AUX_DIR([build/autoconf])

dnl Advertise the location of our ancillary m4 macros.
AC_CONFIG_MACRO_DIR([build/autoconf/m4])


dnl ****************************************************************************
dnl * Enable various compile time options.
dnl ****************************************************************************

dnl Enable verbose build.
AC_ARG_ENABLE([verbosity],
              AS_HELP_STRING([--enable-verbosity],
		             [Echo compilation commands during build (default is "no")]),,
              [enableval=no])

case "$enableval" in
   yes) AC_SUBST(VERBOSE_BUILD,[1]) ;;
   no)  AC_SUBST(VERBOSE_BUILD,[0]) ;;
   *)   AC_MSG_ERROR([Invalid setting for --enable-verbosity. Use "yes" or "no"]) ;;
esac

dnl Enable optimized build.
AC_ARG_ENABLE([optimized],
              AS_HELP_STRING([--enable-optimized],
                             [Build Comma with optimizations enabled (default is "no")]),,
              [enableval=no])

case "$enableval" in
   yes) AC_SUBST(OPTIMIZED_BUILD,[1]) ;;
   no)  AC_SUBST(OPTIMIZED_BUILD,[0]) ;;
   *)   AC_MSG_ERROR([Invalid setting for --enable-optimized. Use "yes" or "no"]) ;;
esac

dnl Enable Doxygen.
AC_ARG_ENABLE([doxygen],
              AS_HELP_STRING([--enable-doxygen],
                             [Build Doxygen documentation (default is "no")]),,
              [enableval=no])

case "$enableval" in
   yes) AC_PATH_PROG([DOXYGEN], [doxygen], [])
        if test -z "$DOXYGEN"; then
           AC_MSG_ERROR([[Could not detect doxygen. Disabling..]])
           AC_SUBST(ENABLE_DOXYGEN,[0])
        else
           AC_SUBST(ENABLE_DOXYGEN,[1])
        fi; ;;
   no)  AC_SUBST(ENABLE_DOXYGEN,[0]) ;;
   *)   AC_MSG_ERROR([Invalid setting for --enable-doxygen. Use "yes" or "no"]) ;;
esac;


dnl Enable Commaspec.
AC_ARG_ENABLE([commaspec],
              AS_HELP_STRING([--enable-commaspec],
                             [Build Commaspec documentation (default is "no")]),,
              [enableval=no])

case "$enableval" in
   yes) AC_PATH_PROG([XSLTPROC], [xsltproc], [])
        if test -z "$XSLTPROC"; then
           AC_MSG_ERROR([[Could not detect xsltproc. Disabling..]])
           AC_SUBST(ENABLE_COMMASPEC,[0])
        else
           AC_SUBST(ENABLE_COMMASPEC,[1])
        fi; ;;
   no)  AC_SUBST(ENABLE_COMMASPEC,[0]) ;;
   *)   AC_MSG_ERROR([Invalid setting for --enable-commaspec. Use "yes" or "no"]) ;;
esac;

dnl ****************************************************************************
dnl * Determine the build, host, and target systems.
dnl ****************************************************************************

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl ****************************************************************************
dnl * Check for programs.
dnl ****************************************************************************

AC_PROG_CC(gcc)
AC_PROG_CXX(g++)

AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_RANLIB

AC_PATH_PROG(RM,   [rm],   [rm])
AC_PATH_PROG(AR,   [ar],   [ar])
AC_PATH_PROG(ECHO, [echo], [echo])
AC_PATH_PROG(SED,  [sed],  [sed])

dnl Check for DejaGNU support.
AC_PATH_PROG(RUNTEST, [runtest], [])

dnl ****************************************************************************
dnl * Check for libraries.
dnl ****************************************************************************

dnl Check for LLVM.
AX_LIB_LLVM([2.7])

dnl ****************************************************************************
dnl * Create the output files.
dnl ****************************************************************************

AC_CONFIG_FILES(Makefile.common)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(lib/Makefile)
AC_CONFIG_FILES(lib/ast/Makefile)
AC_CONFIG_FILES(lib/basic/Makefile)
AC_CONFIG_FILES(lib/codegen/Makefile)
AC_CONFIG_FILES(lib/parser/Makefile)
AC_CONFIG_FILES(lib/runtime/Makefile)
AC_CONFIG_FILES(lib/typecheck/Makefile)
AC_CONFIG_FILES(tools/Makefile)
AC_CONFIG_FILES(tools/driver/Makefile)
AC_CONFIG_FILES(tools/driver/config.h)
AC_CONFIG_FILES(doc/Makefile)
AC_CONFIG_FILES(doc/commaspec/Makefile)
AC_CONFIG_FILES(doc/doxygen/Makefile)
AC_CONFIG_FILES(testsuite/Makefile)

dnl If we are building the doxygen reference, output the config file.
if test "$ENABLE_DOXYGEN" -eq 1 ; then
  AC_CONFIG_FILES(doc/doxygen/doxygen.cfg)
fi;

dnl This must be last.
AC_OUTPUT
