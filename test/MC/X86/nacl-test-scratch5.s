// RUN: not llvm-mc -filetype asm -triple i386-unknown-nacl %s 2>&1 | FileCheck %s

// Extraneous .unscratch directive. The assembler should fail.

.scratch %ecx
.unscratch
// CHECK: No scratch registers specified
.unscratch
