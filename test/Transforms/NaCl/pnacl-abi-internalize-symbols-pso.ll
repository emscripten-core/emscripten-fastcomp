; RUN: opt %s -pnacl-abi-simplify-preopt -S | FileCheck %s

; Checks that PNaCl ABI pre-opt simplification correctly internalizes
; symbols except __pnacl_pso_root.


@__pnacl_pso_root = global i32 123
; CHECK: @__pnacl_pso_root = global i32 123

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
