// RUN: not llvm-mc -filetype asm -triple i386-unknown-linux %s 2>&1 | FileCheck %s

// Tests if .scratch directive is seen if the target is not NaCl.
// Should fail.

// CHECK: unknown directive
.scratch %ecx
