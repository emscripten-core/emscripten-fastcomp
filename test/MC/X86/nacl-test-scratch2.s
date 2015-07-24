// RUN: not llvm-mc -filetype asm -triple i386-unknown-nacl %s

// Tests that the assembler fails if the argument is not a valid
// register
	
.scratch %notaregister
