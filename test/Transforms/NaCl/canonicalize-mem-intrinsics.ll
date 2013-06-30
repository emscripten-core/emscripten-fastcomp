; RUN: opt %s -canonicalize-mem-intrinsics -S | FileCheck %s
; RUN: opt %s -canonicalize-mem-intrinsics -S \
; RUN:     | FileCheck %s -check-prefix=CLEANED

declare void @llvm.memset.p0i8.i64(i8*, i8, i64, i32, i1)
declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i32, i1)
declare void @llvm.memmove.p0i8.p0i8.i64(i8*, i8*, i64, i32, i1)
; CLEANED-NOT: @llvm.mem{{.*}}i64


define void @memset_caller(i8* %dest, i8 %char, i64 %size) {
  call void @llvm.memset.p0i8.i64(i8* %dest, i8 %char, i64 %size, i32 1, i1 0)
  ret void
}
; CHECK: define void @memset_caller
; CHECK-NEXT: %mem_len_truncate = trunc i64 %size to i32
; CHECK-NEXT: call void @llvm.memset.p0i8.i32(i8* %dest, i8 %char, i32 %mem_len_truncate, i32 1, i1 false)


define void @memcpy_caller(i8* %dest, i8* %src, i64 %size) {
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %dest, i8* %src, i64 %size, i32 1, i1 0)
  ret void
}
; CHECK: define void @memcpy_caller
; CHECK-NEXT: %mem_len_truncate = trunc i64 %size to i32
; CHECK-NEXT: call void @llvm.memcpy.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %mem_len_truncate, i32 1, i1 false)


define void @memmove_caller(i8* %dest, i8* %src, i64 %size) {
  call void @llvm.memmove.p0i8.p0i8.i64(i8* %dest, i8* %src, i64 %size, i32 1, i1 0)
  ret void
}
; CHECK: define void @memmove_caller
; CHECK-NEXT: %mem_len_truncate = trunc i64 %size to i32
; CHECK-NEXT: call void @llvm.memmove.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %mem_len_truncate, i32 1, i1 false)


; Check that constant sizes remain as constants.

define void @memset_caller_const(i8* %dest, i8 %char) {
  call void @llvm.memset.p0i8.i64(i8* %dest, i8 %char, i64 123, i32 1, i1 0)
  ret void
}
; CHECK: define void @memset_caller
; CHECK-NEXT: call void @llvm.memset.p0i8.i32(i8* %dest, i8 %char, i32 123, i32 1, i1 false)
