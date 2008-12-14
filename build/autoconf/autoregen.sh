#!/bin/sh

##-- autoregen.sh ------------------------------------------------------------##
#
# This file is distributed under the MIT License.  See LICENSE.txt for
# details.
#
# Copyright (C) 2008, Stephen Wilson
#
##----------------------------------------------------------------------------##

die () {
   echo "$@" 1>&2
   exit 1
}

echo "Regenerating aclocal.m4"
aclocal --force -I `pwd`/m4 || die "aclocal failed"

echo "Regenerating configure"
autoconf --force --warnings=all -o ../../configure configure.ac \
  || die "autoconf failed"

exit 0
