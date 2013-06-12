; RUN: opt -S -nacl-strip-attributes %s | FileCheck %s

define fastcc void @func_attrs(i32 inreg, i32 zeroext) noreturn nounwind readonly {
  ret void
}
; CHECK: define void @func_attrs(i32, i32) {

define void @call_attrs() {
  call fastcc void @func_attrs(i32 inreg 10, i32 zeroext 20) noreturn nounwind readonly
  ret void
}
; CHECK: define void @call_attrs()
; CHECK: call void @func_attrs(i32 10, i32 20){{$}}

; We currently don't attempt to strip attributes from intrinsic
; declarations because the reader automatically inserts attributes
; based on built-in knowledge of intrinsics, so it is difficult to get
; rid of them here.
declare i8* @llvm.nacl.read.tp()
; CHECK: declare i8* @llvm.nacl.read.tp() #{{[0-9]+}}

define void @arithmetic_attrs() {
  %add = add nsw i32 1, 2
  %shl = shl nuw i32 3, 4
  %lshr = lshr exact i32 2, 1
  ret void
}
; CHECK: define void @arithmetic_attrs() {
; CHECK-NEXT: %add = add i32 1, 2
; CHECK-NEXT: %shl = shl i32 3, 4
; CHECK-NEXT: %lshr = lshr i32 2, 1
