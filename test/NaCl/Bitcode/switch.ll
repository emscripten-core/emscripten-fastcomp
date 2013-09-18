; Test that we no longer put VECTOR/ARRAY type entries, associated with
; switch instructions, into the bitcode file.

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

; Test case where we switch on a variable.
define void @SwitchVariable(i32) {
  switch i32 %0, label %l1 [
    i32 1, label %l2
    i32 2, label %l2
    i32 4, label %l3
    i32 5, label %l3
  ]
  br label %end
l1:
  br label %end
l2:
  br label %end
l3:
  br label %end
end:
  ret void
}

; Test case where we switch on a constant.
define void @SwitchConstant(i32) {
  switch i32 3, label %l1 [
    i32 1, label %l2
    i32 2, label %l2
    i32 4, label %l3
    i32 5, label %l3
  ]
  br label %end
l1:
  br label %end
l2:
  br label %end
l3:
  br label %end
end:
  ret void
}

; PF2:      <TYPE_BLOCK_ID>
; PF2-NEXT:   <NUMENTRY op0=3/>
; PF2-NEXT:   <VOID/>
; PF2-NEXT:   <INTEGER op0=32/>
; PF2-NEXT:   <FUNCTION op0={{.*}} op1={{.*}} op2={{.*}}/>
; PF2-NEXT: </TYPE_BLOCK_ID>
