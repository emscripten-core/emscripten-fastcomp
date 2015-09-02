// RUN: not llvm-mc -filetype asm -triple i386-unknown-nacl %s 2>&1 | FileCheck %s

// Tests that the assembler fails if the argument is not a valid
// register

// CHECK: invalid register name
.scratch %notaregister
