// RUN: not llvm-mc -filetype asm -triple i386-unknown-linux %s

// Tests if .scratch directive is seen if the target is not NaCl.
// Should fail.

.scratch %ecx
