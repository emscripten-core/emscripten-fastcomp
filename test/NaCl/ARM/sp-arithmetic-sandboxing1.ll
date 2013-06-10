; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-store -sfi-load -sfi-stack -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define void @foo(i32* %input, i32* %output) nounwind {
entry:
  %input.addr = alloca i32*, align 4
  %output.addr = alloca i32*, align 4
  %temp = alloca i32, align 4

; CHECK:        sub   sp, sp
; CHECK-NEXT:   bic   sp, sp, #3221225472

  store i32* %input, i32** %input.addr, align 4
  store i32* %output, i32** %output.addr, align 4
  %0 = load i32** %input.addr, align 4
  %arrayidx = getelementptr inbounds i32* %0, i32 1
  %1 = load i32* %arrayidx, align 4
  store i32 %1, i32* %temp, align 4
  %2 = load i32* %temp, align 4
  %3 = load i32** %output.addr, align 4
  %arrayidx1 = getelementptr inbounds i32* %3, i32 0
  store i32 %2, i32* %arrayidx1, align 4

; CHECK:        add   sp, sp
; CHECK-NEXT:   bic   sp, sp, #3221225472

  ret void
}
