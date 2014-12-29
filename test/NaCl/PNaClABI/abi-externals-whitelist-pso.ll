; RUN: not pnacl-abicheck < %s | FileCheck %s

@global_var = global [4 x i8] c"abcd"
; CHECK: global_var is not a valid external symbol (disallowed)

; __pnacl_pso_root is whitelisted.
@__pnacl_pso_root = global [4 x i8] c"abcd"
; CHECK-NOT: disallowed
