; RUN: llc -mtriple armv7a-linux-gnueabihf < %s -o - \
; RUN:     | FileCheck %s --check-prefix=EABI
; RUN: llc -mtriple armv7a-linux-gnueabihf < %s -o - \
; RUN:     -arm-enable-aeabi-functions=0 | FileCheck %s --check-prefix=NO_EABI
; RUN: llc -mtriple armv7a-none-nacl-gnueabihf < %s -o - \
; RUN:     | FileCheck %s --check-prefix=NO_EABI

; PNaCl's build of compiler-rt (libgcc.a) does not define __aeabi_*
; functions for ARM yet.  Test that the backend can avoid using these
; __aeabi_* functions.

define i64 @do_division(i64 %a, i64 %b) {
  %div = udiv i64 %a, %b
  ret i64 %div
}
; EABI: bl __aeabi_uldivmod
; NO_EABI: bl __udivdi3
