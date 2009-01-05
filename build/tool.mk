##-- comma/build/comprules.mk --------------------------------*- Makefile -*--##
#
# This file is distributed under the MIT License.  See LICENSE.txt for details.
#
# Copyright (C) 2008, Stephen Wilson
#
##----------------------------------------------------------------------------##
#
# This file contains the rules for compiling executables.
#
# Each tool using this file must define, at a minimum, "tool_name".  This
# variable should hold the name of the desired executable.
#
# In addition, the following two variables may be defined:
#
#   - "comma_components": a sequence of library names provided by the Comma
#     system on which this tool depends.  For example, if you depend on
#     libbasic.a, then comma_components should contain "basic".
#
#   - "llvm_components": a sequence of LLVM components (one of the options
#     produced by the command `llvm-config --components`.  At this time, there
#     is no mechanism to infer the needed components using only the contents of
#     "comma_components", and so must be provided explicitly,
#
##----------------------------------------------------------------------------##

#
# Basic sanity checks
#
ifndef tool_name
  error "tool.mk envoked without a tool name defined!"
endif

#
# Pull in the basic compilation rules.
#
include $(makefiles)/comprules.mk

thetool := $(toolroot)/$(tool_name)

all: toolhome $(thetool)

#
# Determine dependencies on Comma libraries.
#
ifdef comma_components
  comma_libnames := $(addprefix $(libroot)/lib, $(addsuffix .a, $(comma_components)))
  comma_ld_flags := -L$(libroot) $(patsubst %, -l%, $(comma_components))
endif

#
# If llvm_components are defined, use llvm-config to determine the linker flags.
#
ifdef llvm_components
  llvm_ld_flags := $(shell $(LLVM_CONFIG) --ldflags --libs $(llvm_components))
endif

#
# Assemble all linker flags.
#
ld_flags = $(comma_ld_flags) $(llvm_ld_flags)


$(thetool): $(prj_objects) $(comma_libnames)
	$(verb) echo "linking $(tool_name)"; \
	$(CXX) $(prj_objects) $(cxx_flags) $(ld_flags) -o $@

#
# Ensure that the destination for the tool exists.
#
.PHONY: toolhome

toolhome:
	$(verb) set -e;               \
	if [ ! -d $(toolroot) ]; then \
	  $(MKDIR) -p $(toolroot);    \
	fi

#
# Extend the clean target to handle the generated object files and executable.
#
.PHONY: clean-tool

clean: clean-tool

clean-tool:
	$(verb) set -e; \
	rm -f $(prj_objects) $(thetool)
