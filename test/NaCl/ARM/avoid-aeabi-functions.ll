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
; EABI-LABEL: do_division:
; EABI: bl __aeabi_uldivmod
; NO_EABI-LABEL: do_division:
; NO_EABI: bl __udivdi3


declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1)

; __aeabi_memset() has two arguments swapped compared with the normal
; memset(), so check that both are handled correctly.
define void @do_memset(i8* %addr) {
  call void @llvm.memset.p0i8.i32(i8* %addr, i8 255, i32 100, i32 0, i1 false)
  ret void
}
; EABI-LABEL: do_memset:
; EABI: mov r1, #100
; EABI: mov r2, #255
; EABI: bl __aeabi_memset
; NO_EABI-LABEL: do_memset:
; NO_EABI: mov r1, #255
; NO_EABI: mov r2, #100
; NO_EABI: bl memset
