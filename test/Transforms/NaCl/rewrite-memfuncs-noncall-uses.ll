; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s
; Check that the rewrite pass behaves correctly in the presence 
; of various uses of mem* that are not calls.

@fpcpy = global i8* (i8*, i8*, i32)* @memcpy
; CHECK: @fpcpy = global i8* (i8*, i8*, i32)* @memcpy
@fpmove = global i8* (i8*, i8*, i32)* @memmove
; CHECK: @fpmove = global i8* (i8*, i8*, i32)* @memmove
@fpset = global i8* (i8*, i32, i32)* @memset
; CHECK: @fpset = global i8* (i8*, i32, i32)* @memset

; CHECK: define internal i8* @memcpy(i8* %dest, i8* %src, i32 %len) {
; CHECK:   call void @llvm.memcpy.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 1, i1 false)
; CHECK:   ret i8* %dest
; CHECK: }

; CHECK: define internal i8* @memmove(i8* %dest, i8* %src, i32 %len) {
; CHECK:   call void @llvm.memmove.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 1, i1 false)
; CHECK:   ret i8* %dest
; CHECK: }

; CHECK: define internal i8* @memset(i8* %dest, i32 %val, i32 %len) {
; CHECK:   %trunc_byte = trunc i32 %val to i8
; CHECK:   call void @llvm.memset.p0i8.i32(i8* %dest, i8 %trunc_byte, i32 %len, i32 1, i1 false)
; CHECK:   ret i8* %dest
; CHECK: }

declare i8* @memcpy(i8*, i8*, i32)
declare i8* @memmove(i8*, i8*, i32)
declare i8* @memset(i8*, i32, i32)
