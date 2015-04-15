; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64--nacl"

; Test that we can handle 2 memory constraints in one inline asm
define i32 @f(i32* %a, i32 %b) {
entry:
; CHECK: lock
; CHECK: xaddl %e{{[a-z]*}}, %nacl:(%r15,%r{{[a-z]*}})
  %r = call i32 asm sideeffect "lock; xaddl $0, $1", "=r,=*m,0,*m,~{memory},~{dirflag},~{fpsr},~{flags}"(i32* %a, i32 %b, i32* %a)
  ret i32 %r
}
