; RUN: opt %s -pnacl-abi-simplify-postopt -S | FileCheck %s

; "-pnacl-abi-simplify-postopt" runs various passes which are tested
; thoroughly in other *.ll files.  This file is a smoke test to check
; that the passes work together OK.


@var = global i32 256
; CHECK: @var = global [4 x i8]

define i16 @read_var() {
  %val = load i16* bitcast (i32* @var to i16*)
  ret i16 %val
}
; CHECK: = bitcast [4 x i8]* @var
; CHECK-NEXT: load i16*
