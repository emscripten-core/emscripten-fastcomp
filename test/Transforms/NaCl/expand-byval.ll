; RUN: opt -expand-byval %s -S | FileCheck %s

target datalayout = "p:32:32:32"

%MyStruct = type { i32, i8, i32 }
%AlignedStruct = type { double, double }


; Removal of "byval" attribute for passing structs arguments by value

declare void @ext_func(%MyStruct*)

define void @byval_receiver(%MyStruct* byval align 32 %ptr) {
  call void @ext_func(%MyStruct* %ptr)
  ret void
}
; Strip the "byval" and "align" attributes.
; CHECK: define void @byval_receiver(%MyStruct* noalias %ptr) {
; CHECK-NEXT: call void @ext_func(%MyStruct* %ptr)


declare void @ext_byval_func(%MyStruct* byval)
; CHECK: declare void @ext_byval_func(%MyStruct* noalias)

define void @byval_caller(%MyStruct* %ptr) {
  call void @ext_byval_func(%MyStruct* byval %ptr)
  ret void
}
; CHECK: define void @byval_caller(%MyStruct* %ptr) {
; CHECK-NEXT: %ptr.byval_copy = alloca %MyStruct, align 4
; CHECK: call void @llvm.lifetime.start(i64 12, i8* %{{.*}})
; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* %{{.*}}, i8* %{{.*}}, i64 12, i32 4, i1 false)
; CHECK-NEXT: call void @ext_byval_func(%MyStruct* noalias %ptr.byval_copy)


define void @byval_tail_caller(%MyStruct* %ptr) {
  tail call void @ext_byval_func(%MyStruct* byval %ptr)
  ret void
}
; CHECK: define void @byval_tail_caller(%MyStruct* %ptr) {
; CHECK: {{^}} call void @ext_byval_func(%MyStruct* noalias %ptr.byval_copy)


define void @byval_invoke(%MyStruct* %ptr) {
  invoke void @ext_byval_func(%MyStruct* byval align 32 %ptr)
      to label %cont unwind label %lpad
cont:
  ret void
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret void
}
; CHECK: define void @byval_invoke(%MyStruct* %ptr) {
; CHECK: %ptr.byval_copy = alloca %MyStruct, align 32
; CHECK: call void @llvm.lifetime.start(i64 12, i8* %{{.*}})
; CHECK: invoke void @ext_byval_func(%MyStruct* noalias %ptr.byval_copy)
; CHECK: cont:
; CHECK: call void @llvm.lifetime.end(i64 12, i8* %{{.*}})
; CHECK: lpad:
; CHECK: call void @llvm.lifetime.end(i64 12, i8* %{{.*}})


; Check handling of alignment

; Check that "align" is stripped for declarations too.
declare void @ext_byval_func_align(%MyStruct* byval align 32)
; CHECK: declare void @ext_byval_func_align(%MyStruct* noalias)

define void @byval_caller_align_via_attr(%MyStruct* %ptr) {
  call void @ext_byval_func(%MyStruct* byval align 32 %ptr)
  ret void
}
; CHECK: define void @byval_caller_align_via_attr(%MyStruct* %ptr) {
; CHECK-NEXT: %ptr.byval_copy = alloca %MyStruct, align 32
; The memcpy may assume that %ptr is 32-byte-aligned.
; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* %2, i8* %3, i64 12, i32 32, i1 false)

declare void @ext_byval_func_align_via_type(%AlignedStruct* byval)

; %AlignedStruct contains a double so requires an alignment of 8 bytes.
; Looking at the alignment of %AlignedStruct is a workaround for a bug
; in pnacl-clang:
; https://code.google.com/p/nativeclient/issues/detail?id=3403
define void @byval_caller_align_via_type(%AlignedStruct* %ptr) {
  call void @ext_byval_func_align_via_type(%AlignedStruct* byval %ptr)
  ret void
}
; CHECK: define void @byval_caller_align_via_type(%AlignedStruct* %ptr) {
; CHECK-NEXT: %ptr.byval_copy = alloca %AlignedStruct, align 8
; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* %{{.*}}, i8* %{{.*}}, i64 16, i32 8, i1 false)


; Removal of "sret" attribute for returning structs by value

declare void @ext_sret_func(%MyStruct* sret align 32)
; CHECK: declare void @ext_sret_func(%MyStruct*)

define void @sret_func(%MyStruct* sret align 32 %buf) {
  ret void
}
; CHECK: define void @sret_func(%MyStruct* %buf) {

define void @sret_caller(%MyStruct* %buf) {
  call void @ext_sret_func(%MyStruct* sret align 32 %buf)
  ret void
}
; CHECK: define void @sret_caller(%MyStruct* %buf) {
; CHECK-NEXT: call void @ext_sret_func(%MyStruct* %buf)


; Check that other attributes are preserved

define void @inreg_attr(%MyStruct* inreg %ptr) {
  ret void
}
; CHECK: define void @inreg_attr(%MyStruct* inreg %ptr) {

declare void @func_attrs() #0
; CHECK: declare void @func_attrs() #0

attributes #0 = { noreturn nounwind }
; CHECK: attributes #0 = { noreturn nounwind }
