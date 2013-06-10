; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-store -sfi-load -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define void @foo(i32* %input, i32* %output) nounwind {
entry:
  %input.addr = alloca i32*, align 4
  %output.addr = alloca i32*, align 4
  store i32* %input, i32** %input.addr, align 4
  store i32* %output, i32** %output.addr, align 4
  %0 = load i32** %input.addr, align 4
  %1 = load i32* %0, align 4

; CHECK:          bic r0, r0, #3221225472
; CHECK-NEXT:     ldr r0, [r0]

  %add = add nsw i32 %1, 4
  %2 = load i32** %output.addr, align 4
  store i32 %add, i32* %2, align 4

; CHECK:          bic r1, r1, #3221225472
; CHECK-NEXT:     str r0, [r1]

  ret void
}



