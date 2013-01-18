; RUN: opt -verify-pnaclabi-functions -analyze < %s |& FileCheck %s
; Test instruction opcodes allowed by PNaCl ABI

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"

define i32 @terminators() nounwind {
; Terminator instructions
terminators:
 ret i32 0
 br i1 0, label %next2, label %next
next:
 switch i32 1, label %next2 [i32 0, label %next]
next2:
  unreachable
  resume i8 0
  indirectbr i8* undef, [label %next, label %next2]
; CHECK-NOT: disallowed
; CHECK: Function terminators has disallowed instruction: indirectbr
}