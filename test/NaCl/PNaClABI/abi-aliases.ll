; RUN: pnacl-abicheck < %s | FileCheck %s

@aliased_var = internal global [1 x i8] c"x"
; CHECK-NOT: disallowed

@alias1 = alias [1 x i8]* @aliased_var
; CHECK: Variable alias1 is an alias (disallowed)
