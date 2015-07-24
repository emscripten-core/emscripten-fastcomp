// RUN: not llvm-mc -filetype asm -triple i386-unknown-nacl %s

// Extraneous .unscratch directive. The assembler should fail.

.scratch %ecx
.unscratch
.unscratch
