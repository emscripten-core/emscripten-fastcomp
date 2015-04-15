//===- llvm/Support/Unix/Host.inc -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the UNIX Host support.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only generic UNIX code that
//===          is guaranteed to work on *all* UNIX variants.
//===----------------------------------------------------------------------===//

#include "Unix.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/config.h"
#include <cctype>
#include <string>
#if !defined(__native_client__)
#include <sys/utsname.h>
#endif

using namespace llvm;

static std::string getOSVersion() {
#if !defined(__native_client__)
  struct utsname info;

  if (uname(&info))
    return "";

  return info.release;
#else // (__native_client__)
  return "";
#endif // (__native_client__)
}

std::string sys::getDefaultTargetTriple() {
  std::string TargetTripleString(LLVM_DEFAULT_TARGET_TRIPLE);

  // On darwin, we want to update the version to match that of the
  // target.
  std::string::size_type DarwinDashIdx = TargetTripleString.find("-darwin");
  if (DarwinDashIdx != std::string::npos) {
    TargetTripleString.resize(DarwinDashIdx + strlen("-darwin"));
    TargetTripleString += getOSVersion();
  }

  return Triple::normalize(TargetTripleString);
}
