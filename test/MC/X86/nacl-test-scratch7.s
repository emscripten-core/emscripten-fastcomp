// RUN: not llvm-mc -filetype asm -triple i386-unknown-linux %s 2>&1 | FileCheck %s

// Tests if .unscratch directive is seen if the target is not NaCl.
// Should fail.

// CHECK: unknown directive
.unscratch
