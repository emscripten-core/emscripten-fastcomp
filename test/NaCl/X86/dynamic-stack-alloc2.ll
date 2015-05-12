; RUN: llc -mtriple=x86_64-nacl %s -o - | FileCheck %s

target datalayout = "e-p:32:32-i64:64-n32"
target triple = "le32-unknown-nacl"

; CHECK-LABEL: @foo
; CHECK: .bundle_lock
; CHECK: leal -16({{.*}}), %esp
; CHECK: addq %r15, %rsp
; CHECK: .bundle_unlock
define hidden void @foo() {
entry:
  br label %bb1
; The alloca must be in a non-entry block so it gets lowered in the dag as
; a dynamic_stackalloc node
bb1:
  %0 = alloca i8, i32 16, align 16
  %1 = load i8, i8* %0, align 1
  %call5 = call i32 @bar(i8 %1)
  unreachable
}

declare hidden i32 @bar(i8)