; RUN: pnacl-abicheck < %s | FileCheck %s
; Test type-checking in function bodies. This test is not intended to verify
; all the rules about the various types, but instead to make sure that types
; stashed in various places in function bodies are caught.

@a2 = private global i17 zeroinitializer

; CHECK: Function func has disallowed type: void (i15)
declare void @func(i15 %arg)

!llvm.foo = !{!0}
!0 = metadata !{ half 0.0}

define void @types() {
; CHECK: bad result type: {{.*}} fptrunc
  %h1 = fptrunc double undef to half

; CHECK: bad operand: {{.*}} bitcast half
  %h2 = bitcast half 0.0 to i16

; see below...
  %h3 = fadd double 0.0, fpext (half 0.0 to double)

; CHECK: bad pointer: store
  store i32 0, i32* bitcast (i17* @a2 to i32*), align 1

; CHECK: bad function callee operand: call void @func(i15 1)
  call void @func(i15 1)

; CHECK: Function types has disallowed instruction metadata: !foo
  ret void, !foo !0
}
; CHECK-NOT: disallowed


; TODO:
; the bitcode reader seems to expand some operations inline
; (e.g. fpext, sext, uitofp) such that doing something like
;   %h3 = fadd double 0.0, fpext (half 0.0 to double)
; means the verifier pass will never see the fpext or its operands
