; RUN: not pnacl-abicheck < %s | FileCheck %s

; This is not a valid entry point because it's declared "internal".
define internal void @_start(i32 %arg) {
  ret void
}

; CHECK: Module has no entry point (disallowed)
