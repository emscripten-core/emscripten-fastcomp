; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-branch -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define i32 @foobar(i32 %aa, i32 %bb, i32 (i32)* %f) nounwind {
entry:
  %aa.addr = alloca i32, align 4
  %bb.addr = alloca i32, align 4
  %f.addr = alloca i32 (i32)*, align 8
  %0 = load i32 (i32)** %f.addr, align 8
  %1 = load i32* %aa.addr, align 4
  %call1 = call i32 %0(i32 %1)
; CHECK: bic	r1, r1, #3221225487
; CHECK-NEXT: blx r1
  ret i32 %call1
}


