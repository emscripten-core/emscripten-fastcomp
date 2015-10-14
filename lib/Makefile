##===- lib/Makefile ----------------------------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##
LEVEL = ..

include $(LEVEL)/Makefile.config

PARALLEL_DIRS := IR AsmParser Bitcode Analysis Transforms CodeGen Target      \
                 ExecutionEngine Linker LTO MC Object Option DebugInfo        \
                 IRReader LineEditor ProfileData Passes LibDriver

# @LOCALMOD-BEGIN
ifeq ($(PNACL_BROWSER_TRANSLATOR),1)
  PARALLEL_DIRS := $(filter-out Archive Linker, \
                $(PARALLEL_DIRS))
endif
# @LOCALMOD-END

include $(LEVEL)/Makefile.common
