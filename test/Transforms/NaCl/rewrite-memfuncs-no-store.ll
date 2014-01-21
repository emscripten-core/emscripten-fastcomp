; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s
; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s -check-prefix=CLEANED

declare i8* @memcpy(i8*, i8*, i32)
declare i8* @memmove(i8*, i8*, i32)
declare i8* @memset(i8*, i32, i32)

; No declaration or definition of the library functions should remain, since
; the only uses of mem* functions are calls.
; CLEANED-NOT: @memcpy
; CLEANED-NOT: @memmove
; CLEANED-NOT: @memset

define i8* @call_memcpy(i8* %dest, i8* %src, i32 %len) {
  %result = call i8* @memcpy(i8* %dest, i8* %src, i32 %len)
  ret i8* %result
; CHECK: call void @llvm.memcpy.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 1, i1 false)
; CHECK: ret i8* %dest
}

define i8* @call_memmove(i8* %dest, i8* %src, i32 %len) {
  %result = call i8* @memmove(i8* %dest, i8* %src, i32 %len)
  ret i8* %result
; CHECK: call void @llvm.memmove.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 1, i1 false)
; CHECK: ret i8* %dest
}

define i8* @call_memset(i8* %dest, i32 %c, i32 %len) {
  %result = call i8* @memset(i8* %dest, i32 %c, i32 %len)
  ret i8* %result
; CHECK: %trunc_byte = trunc i32 %c to i8
; CHECK: call void @llvm.memset.p0i8.i32(i8* %dest, i8 %trunc_byte, i32 %len, i32 1, i1 false)
; CHECK: ret i8* %dest
}
