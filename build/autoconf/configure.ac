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

AC_INIT([[Comma]],[[0.01]],[wilsons@start.ca])

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

AC_CHECK_PROG(XSLTPROC, xsltproc, xsltproc)


dnl ****************************************************************************
dnl * Create the output files.
dnl ****************************************************************************

AC_CONFIG_FILES(Makefile.common)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(doc/Makefile)
AC_CONFIG_FILES(doc/commaspec/Makefile)

dnl This must be last.
AC_OUTPUT
