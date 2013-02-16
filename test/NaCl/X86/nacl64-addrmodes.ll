; RUN: llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck %s

; Check that we don't try to fold a negative displacement into a memory
; reference
define i16 @negativedisp(i32 %b) {
; CHECK: negativedisp
  %a = alloca [1 x i16], align 2
  %add = add nsw i32 1073741824, %b
  %arrayidx = getelementptr inbounds [1 x i16]* %a, i32 0, i32 %add
; CHECK-NOT: nacl:-2147483648(
  %c = load i16* %arrayidx, align 2
  ret i16 %c
}
