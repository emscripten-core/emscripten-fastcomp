; Test that we no longer put VECTOR/ARRAY type entries, associated with
; switch instructions, into the bitcode file.

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=1 \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=2 \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s

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

; CHECK:      <TYPE_BLOCK_ID>
; CHECK-NEXT:   <NUMENTRY op0=4/>
; CHECK-NEXT:   <VOID/>
; CHECK-NEXT:   <INTEGER op0=32/>
; CHECK-NEXT:   <FUNCTION op0={{.*}} op1={{.*}} op2={{.*}}/>
; CHECK-NEXT:   <POINTER op0={{.*}} op1={{.*}}/>
; CHECK-NEXT: </TYPE_BLOCK_ID>

