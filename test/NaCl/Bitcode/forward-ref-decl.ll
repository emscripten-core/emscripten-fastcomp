; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump | FileCheck %s

; Test that FORWARDTYPEREF declarations are emitted in the correct
; places.  These are emitted for forward value references inside
; functions.

define external void @_start(i32 %arg) {
; CHECK: <FUNCTION_BLOCK

  br label %bb1
; CHECK: <INST_BR

bb2:
  ; This instruction contains two forward references, because %x and
  ; %y are defined later in the function.
  add i32 %forward1, %forward2
; CHECK-NEXT: <FORWARDTYPEREF abbrevid=
; CHECK-NEXT: <FORWARDTYPEREF abbrevid=
; CHECK-NEXT: <INST_BINOP abbrevid=

  ; The FORWARDTYPEREF declaration should only be emitted once per
  ; value, so the following references will not emit more of them.
  add i32 %forward1, %forward2
; CHECK-NEXT: <INST_BINOP abbrevid=

  ; Test another case of a forward reference.
  call void @_start(i32 %forward3)
; CHECK-NEXT: <FORWARDTYPEREF abbrevid=
; CHECK-NEXT: <INST_CALL

  ; Test that FORWARDTYPEREF is generated for phi nodes (since phi
  ; node operands are a special case in the writer).
  br label %bb3
bb3:
  phi i32 [ %forward4, %bb2 ]
; CHECK-NEXT: <INST_BR
; CHECK-NEXT: <FORWARDTYPEREF abbrevid=
; CHECK-NEXT: <INST_PHI

  ; Test that FORWARDTYPEREF is generated for switch instructions
  ; (since switch condition operands are a special case in the
  ; writer).
  switch i32 %forward5, label %bb4 [i32 0, label %bb4]
bb4:
; CHECK-NEXT: <FORWARDTYPEREF abbrevid=
; CHECK-NEXT: <INST_SWITCH

  ret void
; CHECK-NEXT: <INST_RET

bb1:
  %forward1 = add i32 %arg, 100
  %forward2 = add i32 %arg, 200
  %forward3 = add i32 %arg, 300
  %forward4 = add i32 %arg, 400
  %forward5 = add i32 %arg, 500
  br label %bb2
}
