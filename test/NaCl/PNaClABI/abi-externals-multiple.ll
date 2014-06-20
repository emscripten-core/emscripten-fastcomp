; RUN: not pnacl-abicheck < %s | FileCheck %s

define external void @_start() {
  ret void
}

@__pnacl_pso_root = global [4 x i8] c"abcd"

; CHECK: Module has multiple entry points (disallowed)
