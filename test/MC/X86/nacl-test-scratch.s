// RUN: not llvm-mc -filetype asm -triple i386-unknown-nacl %s

// Tests that a bare .unscratch fails, since there are no scratch
// registers specified

.unscratch
