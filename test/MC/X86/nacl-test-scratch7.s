// RUN: not llvm-mc -filetype asm -triple i386-unknown-linux %s

// Tests if .unscratch directive is seen if the target is not NaCl.
// Should fail.

.unscratch
