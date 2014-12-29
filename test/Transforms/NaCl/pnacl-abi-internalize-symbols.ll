; RUN: opt %s -pnacl-abi-simplify-preopt -S | FileCheck %s

; Checks that PNaCl ABI pre-opt simplification correctly internalizes
; symbols except _start.


@global_var = global [4 x i8] c"abcd"
; CHECK: @global_var = internal global [4 x i8] c"abcd"


define void @main() {
; CHECK: define internal void @main
  ret void
}

define external void @foobarbaz() {
; CHECK: define internal void @foobarbaz
  ret void
}

define void @_start() {
; CHECK: define void @_start
  ret void
}

