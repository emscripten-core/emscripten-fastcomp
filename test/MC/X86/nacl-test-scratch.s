// RUN: not llvm-mc -filetype asm -triple i386-unknown-nacl %s 2>&1 | FileCheck %s

// Tests that a bare .unscratch fails, since there are no scratch
// registers specified

// CHECK: No scratch registers specified
.unscratch
