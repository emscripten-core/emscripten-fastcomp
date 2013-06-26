; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump | FileCheck %s

; Test that alloca's size operand is represented with a relative value
; ID, the same as other instructions' operands.

define external void @_start(i32 %arg) {
; CHECK: <FUNCTION_BLOCK
; CHECK: </CONSTANTS_BLOCK>

  %size = mul i32 %arg, 4
; CHECK-NEXT: <INST_BINOP
  alloca i8, i32 %size
; CHECK-NEXT: <INST_ALLOCA op0=1

  ; Since the operand reference is a relative ID, references to %size
  ; go up by 1 with each instruction.
  alloca i8, i32 %size
; CHECK-NEXT: <INST_ALLOCA op0=2
  alloca i8, i32 %size
; CHECK-NEXT: <INST_ALLOCA op0=3

  ; Reference to a Constant operand.
  alloca i8, i32 256
; CHECK-NEXT: <INST_ALLOCA op0=5

  ret void
; CHECK-NEXT: <INST_RET
}
