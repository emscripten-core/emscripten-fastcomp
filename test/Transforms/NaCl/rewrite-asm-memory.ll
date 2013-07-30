; RUN: opt < %s -rewrite-asm-directives -S | FileCheck %s
; RUN: opt < %s -O3 -rewrite-asm-directives -S | FileCheck %s
; RUN: opt < %s -O3 -rewrite-asm-directives -S | FileCheck %s -check-prefix=ELIM
; RUN: opt < %s -rewrite-asm-directives -S | FileCheck %s -check-prefix=CLEANED

; Test that asm("":::"memory"), a compiler barrier, gets rewritten to a
; sequentially-consistent fence. The test is also run at O3 to make sure
; that loads and stores don't get unexpectedly eliminated.

; CLEANED-NOT: asm

@a = external global i32
@b = external global i32

; Different triples encode "touch everything" constraints differently.
define void @memory_assembly_encoding_test() {
; CHECK: @memory_assembly_encoding_test()
  call void asm sideeffect "", "~{memory}"()
  call void asm sideeffect "", "~{memory},~{dirflag},~{fpsr},~{flags}"()
  ; CHECK-NEXT: fence seq_cst
  ; CHECK-NEXT: fence seq_cst

  ret void
  ; CHECK-NEXT: ret void
}

define void @memory_assembly_ordering_test() {
; CHECK: @memory_assembly_ordering_test()
  %1 = load i32* @a, align 4
  store i32 %1, i32* @b, align 4
  call void asm sideeffect "", "~{memory}"()
  ; CHECK-NEXT: %1 = load i32* @a, align 4
  ; CHECK-NEXT: store i32 %1, i32* @b, align 4
  ; CHECK-NEXT: fence seq_cst

  ; Redundant load from the previous location, and store to the same
  ; location (making the previous one dead). Shouldn't get eliminated
  ; because of the fence.
  %2 = load i32* @a, align 4
  store i32 %2, i32* @b, align 4
  call void asm sideeffect "", "~{memory}"()
  ; CHECK-NEXT: %2 = load i32* @a, align 4
  ; CHECK-NEXT: store i32 %2, i32* @b, align 4
  ; CHECK-NEXT: fence seq_cst

  ; Same here.
  %3 = load i32* @a, align 4
  store i32 %3, i32* @b, align 4
  ; CHECK-NEXT: %3 = load i32* @a, align 4
  ; CHECK-NEXT: store i32 %3, i32* @b, align 4

  ret void
  ; CHECK-NEXT: ret void
}

; Same function as above, but without the barriers. At O3 some loads and
; stores should get eliminated.
define void @memory_ordering_test() {
; ELIM: @memory_ordering_test()
  %1 = load i32* @a, align 4
  store i32 %1, i32* @b, align 4
  %2 = load i32* @a, align 4
  store i32 %2, i32* @b, align 4
  %3 = load i32* @a, align 4
  store i32 %3, i32* @b, align 4
  ; ELIM-NEXT: %1 = load i32* @a, align 4
  ; ELIM-NEXT: store i32 %1, i32* @b, align 4

  ret void
  ; ELIM-NEXT: ret void
}
