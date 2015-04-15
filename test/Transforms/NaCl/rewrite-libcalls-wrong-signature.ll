; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s
; Check how the pass behaves in the presence of library functions with wrong
; signatures.

declare i8 @longjmp(i64)

@flongjmp = global i8 (i64)* @longjmp
; CHECK: @flongjmp = global i8 (i64)* bitcast (void (i64*, i32)* @longjmp to i8 (i64)*)

; CHECK: define internal void @longjmp(i64* %env, i32 %val)

declare i8* @memcpy(i32)

define i8* @call_bad_memcpy(i32 %arg) {
  %result = call i8* @memcpy(i32 %arg)
  ret i8* %result
}

; CHECK: define i8* @call_bad_memcpy(i32 %arg) {
; CHECK:   %result = call i8* bitcast (i8* (i8*, i8*, i32)* @memcpy to i8* (i32)*)(i32 %arg)

declare i8 @setjmp()

; This simulates a case where the original C file had a correct setjmp
; call but due to linking order a wrong declaration made it into the
; IR. In this case, the correct call is bitcasted to the correct type.
; The pass should treat this properly by creating a direct intrinsic
; call instead of going through the wrapper.
define i32 @call_valid_setjmp(i64* %buf) {
  %result = call i32 bitcast (i8 ()* @setjmp to i32 (i64*)*)(i64* %buf)
  ret i32 %result
}

; CHECK:      define i32 @call_valid_setjmp(i64* %buf) {
; CHECK-NEXT:   %jmp_buf_i8 = bitcast i64* %buf to i8*
; CHECK-NEXT:   %result = call i32 @llvm.nacl.setjmp(i8* %jmp_buf_i8)
; CHECK-NEXT:   ret i32 %result
; CHECK-NEXT: }
