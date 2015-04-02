; RUN: opt -S -nacl-strip-attributes %s 2>&1 | FileCheck %s


; Check that we emit a warning for some special meaning sections:
; CHECK: Warning: func_init_array will have its section (.init_array) stripped.
; CHECK-NOT: Warning: __rustc_debug_gdb_scripts_section__ will have its section

@var = unnamed_addr global i32 0
; CHECK: @var = global i32 0

@__rustc_debug_gdb_scripts_section__ = internal unnamed_addr constant [34 x i8] c"\01gdb_load_rust_pretty_printers.py\00", section ".debug_gdb_scripts", align 1
; CHECK: @__rustc_debug_gdb_scripts_section__ = internal constant [34 x i8] c"\01gdb_load_rust_pretty_printers.py\00", align 1

define void @func_section() section ".some_section" {
  ret void
}
; CHECK-LABEL: define void @func_section() {

define void @func_init_array() section ".init_array" {
  ret void
}
; CHECK-LABEL: define void @func_init_array() {


define fastcc void @func_attrs(i32 inreg, i32 zeroext)
    unnamed_addr noreturn nounwind readonly align 8 {
  ret void
}
; CHECK-LABEL: define void @func_attrs(i32, i32) {

define hidden void @hidden_visibility() {
  ret void
}
; CHECK-LABEL: define void @hidden_visibility() {

define protected void @protected_visibility() {
  ret void
}
; CHECK-LABEL: define void @protected_visibility() {


define void @call_attrs() {
  call fastcc void @func_attrs(i32 inreg 10, i32 zeroext 20) noreturn nounwind readonly
  ret void
}
; CHECK-LABEL: define void @call_attrs()
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
; CHECK-LABEL: define void @arithmetic_attrs() {
; CHECK-NEXT: %add = add i32 1, 2
; CHECK-NEXT: %shl = shl i32 3, 4
; CHECK-NEXT: %lshr = lshr i32 2, 1
