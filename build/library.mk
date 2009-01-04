##-- comma/build/library.mk ----------------------------------*- Makefile -*--##
#
# This file is distributed under the MIT License.  See LICENSE.txt for details.
#
# Copyright (C) 2008, Stephen Wilson
#
##----------------------------------------------------------------------------##
#
# This file contains the rules needed to build a static library.  The assumption
# is that all of the files which constitute the library are resident under
# $(prj_srcdir).
#
##----------------------------------------------------------------------------##

#
# Provide a default library name of the form libDIR.a where DIR is the projects
# source directory name.
#
ifndef libname

libname := $(subst liblib,lib,lib$(notdir $(prj_srcdir)).a)

endif

thelib  := $(libroot)/$(libname)

all: libhome $(thelib)

#
# Bring in the generic rules.
#
include $(makefiles)/comprules.mk

#
# Create the library from the compiled objects.
#
$(thelib): $(prj_objects)
	$(verb) echo "forming $(notdir $(thelib))" ; \
	$(RM) -f $(thelib);                          \
	ar -rc $(thelib) $(prj_objects);             \
	$(RANLIB) $(thelib)

#
# Ensure that the destination for the library exists.
#
.PHONY: libhome

libhome:
	$(verb) set -e;              \
        if [ ! -d $(libroot) ]; then \
	  $(MKDIR) -p $(libroot);    \
	fi

#
# Extend the clean target to handle the generated archive.
#
.PHONY: clean-lib

clean: clean-lib

clean-lib:
	$(verb) set -e; \
	$(RM) -f $(thelib)

