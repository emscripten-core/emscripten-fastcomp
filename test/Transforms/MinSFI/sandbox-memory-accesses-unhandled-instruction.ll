; RUN: not opt %s -minsfi-sandbox-memory-accesses -S 2>&1 | FileCheck %s

; The SandboxMemoryAccess pass should fail if it encounters an unexpected 
; instruction such as this 'atomicrmw'. This mechanism protects MinSFI 
; from unsafe operations which it does not handle appearing in the bitcode. 
; This could be a result of a bug in the compiler or a newly introduced
; LLVM instruction.

define i32 @test_unhandled_instr(i32* %ptr) {
  %old = atomicrmw add i32* %ptr, i32 1 acquire
  ret i32 %old
}

; CHECK: LLVM ERROR: SandboxMemoryAccesses: unexpected instruction with pointer-type operands
