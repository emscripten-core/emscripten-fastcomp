//===--- Unix/Watchdog.inc - Unix Watchdog Implementation -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the generic Unix implementation of the Watchdog class.
//
//===----------------------------------------------------------------------===//

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace llvm {
  namespace sys {
    Watchdog::Watchdog(unsigned int seconds) {
#ifdef HAVE_UNISTD_H
  #ifndef __native_client__
      alarm(seconds);
  #endif // __native_client__
#endif
    }

    Watchdog::~Watchdog() {
#ifdef HAVE_UNISTD_H
  #ifndef __native_client__
      alarm(0);
  #endif //__native_client__
#endif
    }
  }
}
