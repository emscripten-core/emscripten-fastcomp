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
