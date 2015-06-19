#===- tools/pnacl-llc/Makefile -----------------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL := ../..
TOOLNAME := pnacl-llc
LINK_COMPONENTS := all-targets bitreader naclbitreader irreader \
                   asmparser naclanalysis naclbittestutils nacltransforms

include $(LEVEL)/Makefile.common

ifeq ($(PNACL_BROWSER_TRANSLATOR),1)
  # NOTE: we specify --noirt to tell the driver that we should link
  # against private (non-stable, non-IRT) libraries for the
  # sandboxed translator.  This could have been specified directly,
  # except that LLVM slips in -lpthread elsewhere in the build system,
  # and we need it to use -lpthread_private instead.
  LIBS += -Wl,--noirt -lsrpc -limc_syscalls -lplatform -lgio -lpthread
endif
