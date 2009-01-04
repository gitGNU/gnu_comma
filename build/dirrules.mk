##-- comma/build/dirrules.mk ---------------------------------*- Makefile -*--##
#
# This file is distributed under the MIT License.  See LICENSE.txt for details.
#
# Copyright (C) 2008, Stephen Wilson
#
##----------------------------------------------------------------------------##
#
# These are the Makefile rules which control the building of several sub
# directories.  All that is required is for the DIRS variable to contain all sub
# directories.  The directories will be built and cleaned in the order in which
# they appear.
#
##----------------------------------------------------------------------------##

ifeq ($(DIRS),)
	$(error DIRS variable not set in dirrules.mk)
endif

src_dirs := $(addprefix $(prj_srcdir)/, $(DIRS))
obj_dirs := $(addprefix $(prj_objdir)/, $(DIRS))

%:
	$(verb) set -e;            \
	for dir in $(obj_dirs); do \
	   $(MAKE) -C $$dir $*;    \
	done

.PHONY: clean

clean:
	$(verb) set -e;            \
	for dir in $(obj_dirs); do \
	   $(MAKE) -C $$dir clean; \
	done;
