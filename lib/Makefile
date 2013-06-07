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

PARALLEL_DIRS := IR IRReader AsmParser Bitcode Archive Analysis Transforms CodeGen \
                Target ExecutionEngine Linker MC Object Option Wrap DebugInfo

ifeq ($(NACL_SANDBOX),1)
  PARALLEL_DIRS := $(filter-out Archive Linker, \
                $(PARALLEL_DIRS))
endif

include $(LEVEL)/Makefile.common

