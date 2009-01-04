#####
#
# SYNOPSIS
#
#   AX_LIB_LLVM([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   This macro provides tests of availability of the LLVM library
#   of particular version or newer.
#
#   The AX_LIB_LLVM macro takes only one argument which is optional. If
#   there is no required version passed, then macro does not run
#   version test.
#
#   The --with-llvm option takes one of three possible values:
#
#   no - do not check for LLVM library
#
#   yes - do check for the LLVM library in standard locations
#   (llvm-config should be in the PATH)
#
#   path - complete path to the llvm-config utility, use this option if
#   llvm-config can't be found in the PATH.
#
#   This macro calls:
#
#     AC_SUBST(LLVM_CONFIG)
#     AC_SUBST(LLVM_VERSION)
#
#   And sets:
#
#     HAVE_LLVM
#
# LAST MODIFICATION
#
#   2008-01-12
#
# COPYLEFT
#
#   Copyright (c) 2008 Stephen Wilson <wilsons@multiboard.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice and
#   this notice are preserved.

AC_DEFUN([AX_LIB_LLVM],
[
    AC_ARG_WITH([llvm],
        AS_HELP_STRING([--with-llvm=@<:@ARG@:>@],
            [use LLVM @<:@default=yes@:>@, optionally specify path to llvm-config]
        ),
        [
        if test "$withval" = "no"; then
            want_llvm="no"
        elif test "$withval" = "yes"; then
            want_llvm="yes"
        else
            want_llvm="yes"
            LLVM_CONFIG="$withval"
        fi
        ],
        [want_llvm="yes"]
    )

    LLVM_CFLAGS=""
    LLVM_LDFLAGS=""
    LLVM_VERSION=""

    dnl
    dnl Check LLVM libraries
    dnl

    if test "$want_llvm" = "yes"; then

        if test -z "$LLVM_CONFIG" -o test; then
            AC_PATH_PROG([LLVM_CONFIG], [llvm-config], [no])
        fi

        if test "$LLVM_CONFIG" != "no"; then
            AC_MSG_CHECKING([for LLVM libraries])

            LLVM_VERSION=`$LLVM_CONFIG --version`

            AC_DEFINE([HAVE_LLVM], [1],
                [Define to 1 if LLVM libraries are available])

            found_llvm="yes"
            AC_MSG_RESULT([yes])
        else
            found_llvm="no"
            AC_MSG_RESULT([no])
        fi
    fi

    dnl
    dnl Check if required version of LLVM is available
    dnl

    llvm_version_req=ifelse([$1], [], [], [$1])

    if test "$found_llvm" = "yes" -a -n "$llvm_version_req"; then

        AC_MSG_CHECKING([if LLVM version is >= $llvm_version_req])

        dnl Decompose required version string of LLVM
        dnl and calculate its numeric representation.
        llvm_version_req_major=`expr $llvm_version_req : '\([[0-9]]*\)'`
        llvm_version_req_minor=`expr $llvm_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        llvm_version_req_micro=`expr $llvm_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$llvm_version_req_micro" = "x"; then
            llvm_version_req_micro="0"
        fi

        llvm_version_req_number=`expr $llvm_version_req_major \* 1000000 \
                                   \+ $llvm_version_req_minor \* 1000 \
                                   \+ $llvm_version_req_micro`

        dnl Decompose version string of installed LLVM
        dnl and calculate its numeric representation.
        llvm_version_major=`expr $LLVM_VERSION : '\([[0-9]]*\)'`
        llvm_version_minor=`expr $LLVM_VERSION : '[[0-9]]*\.\([[0-9]]*\)'`
        llvm_version_micro=`expr $LLVM_VERSION : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$llvm_version_micro" = "x"; then
            llvm_version_micro="0"
        fi

        llvm_version_number=`expr $llvm_version_major \* 1000000 \
                                   \+ $llvm_version_minor \* 1000 \
                                   \+ $llvm_version_micro`

        llvm_version_check=`expr $llvm_version_number \>\= $llvm_version_req_number`
        if test "$llvm_version_check" = "1"; then
            AC_MSG_RESULT([yes])
        else
            AC_MSG_RESULT([no])
        fi
    fi

    if test "$found_llvm" = "no"; then
        AC_MSG_ERROR([[We could not detect the LLVM libraries.]])
    fi

    if test "$llvm_version_check" != "1"; then
        AC_MSG_ERROR([[We could not detect the LLVM libraries (version $llvm_version_req).]])
    fi

    AC_SUBST([LLVM_VERSION])
    AC_SUBST([LLVM_CONFIG])
])
