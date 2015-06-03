; RUN: llc -O0 < %s | FileCheck %s
target datalayout = "e-p:32:32-i64:64-n32"
target triple = "armv7-unknown-nacl-gnueabihf"

declare void @llvm.eh.return.i32(i32, i8*) nounwind
declare void @llvm.eh.unwind.init() nounwind

; CHECK-LABEL: @raise_exception
; CHECK: push {r0, r1, r4, r5, r6, r7, r8, r10, r11, lr}
define i32 @raise_exception(i32 %b) {
entry:
  tail call void @llvm.eh.unwind.init()
  %a = add i32 %b, 3
  %cmp = icmp slt i32 %a, 0
  br i1 %cmp, label %normal_ret, label %eh_ret
normal_ret:
; CHECK: ldm sp!, {r12}
; CHECK: ldm sp!, {r12}
; CHECK: pop {r4, r5, r6, r7, r8, r10, r11, lr}
  ret i32 %a
eh_ret:
; CHECK: pop {r0, r1, r4, r5, r6, r7, r8, r10, r11, lr}
  tail call void @llvm.eh.return.i32(i32 %a, i8* null)
  unreachable
}