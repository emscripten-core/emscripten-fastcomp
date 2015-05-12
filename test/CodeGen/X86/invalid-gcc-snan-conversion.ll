; If LLVM is built in Release mode with a buggy gcc under x86-32, it
; may transform 64-bit constants with a signaling NaN bit pattern into
; a quiet NaN bit pattern.  See
; http://gcc.gnu.org/bugzilla/show_bug.cgi?id=58416
 
; RUN: llc -march=x86-64 < %s | FileCheck %s

define i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %tmp = alloca i64, align 8
  store i32 0, i32* %retval
; -4503599627370495 == 0xfff0000000000001
  store i64 -4503599627370495, i64* %tmp, align 8
  %0 = load i64, i64* %tmp, align 8
  call void @Consume(i64 %0)
  ret i32 0
}

; CHECK: main:
; make sure 0xfff0000000000001 didn't change to 0xfff8000000000001
; CHECK: 0xFFF00000
; CHECK-NOT: 0xFFF80000

declare void @Consume(i64) #1
