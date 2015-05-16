; RUN: opt %s -pnacl-abi-simplify-postopt -S | FileCheck %s
; RUN: opt %s -pnacl-abi-simplify-postopt -S \
; RUN:     | FileCheck %s -check-prefix=CLEANUP

; "-pnacl-abi-simplify-postopt" runs various passes which are tested
; thoroughly in other *.ll files.  This file is a smoke test to check
; that the passes work together OK.

target datalayout = "p:32:32:32"

@var = global i32 256
; CHECK: @var = global [4 x i8]

define i16 @read_var() {
  %val = load i16, i16* bitcast (i32* @var to i16*)
  ret i16 %val
}
; CHECK: = bitcast [4 x i8]* @var
; CHECK-NEXT: load i16, i16*

; Check that dead prototypes are successfully removed.
declare void @unused_prototype(i8*)
; CLEANUP-NOT: unused_prototype
