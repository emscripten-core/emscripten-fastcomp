; RUN: opt < %s -nacl-rewrite-atomics -remove-asm-memory -S | \
; RUN:       FileCheck %s
; RUN: opt < %s -O3 -nacl-rewrite-atomics -remove-asm-memory -S | \
; RUN:       FileCheck %s
; RUN: opt < %s -O3 -nacl-rewrite-atomics -remove-asm-memory -S | \
; RUN:       FileCheck %s -check-prefix=ELIM
; RUN: opt < %s -nacl-rewrite-atomics -remove-asm-memory -S | \
; RUN:       FileCheck %s -check-prefix=CLEANED

; ``asm("":::"memory")`` is used as a compiler barrier and the GCC-style
; builtin ``__sync_synchronize`` is intended as a barrier for all memory
; that could be observed by external threads. They both get rewritten
; for NaCl by Clang to a sequentially-consistent fence surrounded by
; ``call void asm sideeffect "", "~{memory}"``.
;
; The test is also run at O3 to make sure that non-volatile and
; non-atomic loads and stores to escaping objects (i.e. loads and stores
; which could be observed by other threads) don't get unexpectedly
; eliminated.

; CLEANED-NOT: asm

target datalayout = "p:32:32:32"

@a = external global i32
@b = external global i32

; Different triples encode ``asm("":::"memory")``'s "touch everything"
; constraints differently.  They should get detected and removed.
define void @memory_assembly_encoding_test() {
; CHECK: @memory_assembly_encoding_test()
  call void asm sideeffect "", "~{memory}"()
  call void asm sideeffect "", "~{memory},~{dirflag},~{fpsr},~{flags}"()
  call void asm sideeffect "", "~{foo},~{memory},~{bar}"()

  ret void
  ; CHECK-NEXT: ret void
}

define void @memory_assembly_ordering_test() {
; CHECK: @memory_assembly_ordering_test()
  %1 = load i32, i32* @a, align 4
  store i32 %1, i32* @b, align 4
  call void asm sideeffect "", "~{memory}"()
  fence seq_cst
  call void asm sideeffect "", "~{memory}"()
  ; CHECK-NEXT: %1 = load i32, i32* @a, align 4
  ; CHECK-NEXT: store i32 %1, i32* @b, align 4
  ; CHECK-NEXT: call void @llvm.nacl.atomic.fence.all()

  ; Redundant load from the previous location, and store to the same
  ; location (making the previous one dead). Shouldn't get eliminated
  ; because of the fence.
  %2 = load i32, i32* @a, align 4
  store i32 %2, i32* @b, align 4
  call void asm sideeffect "", "~{memory}"()
  fence seq_cst
  call void asm sideeffect "", "~{memory}"()
  ; CHECK-NEXT: %2 = load i32, i32* @a, align 4
  ; CHECK-NEXT: store i32 %2, i32* @b, align 4
  ; CHECK-NEXT: call void @llvm.nacl.atomic.fence.all()

  ; Same here.
  %3 = load i32, i32* @a, align 4
  store i32 %3, i32* @b, align 4
  ; CHECK-NEXT: %3 = load i32, i32* @a, align 4
  ; CHECK-NEXT: store i32 %3, i32* @b, align 4

  ret void
  ; CHECK-NEXT: ret void
}

; Same function as above, but without the barriers. At O3 some loads and
; stores should get eliminated.
define void @memory_ordering_test() {
; ELIM: @memory_ordering_test()
  %1 = load i32, i32* @a, align 4
  store i32 %1, i32* @b, align 4
  %2 = load i32, i32* @a, align 4
  store i32 %2, i32* @b, align 4
  %3 = load i32, i32* @a, align 4
  store i32 %3, i32* @b, align 4
  ; ELIM-NEXT: %1 = load i32, i32* @a, align 4
  ; ELIM-NEXT: store i32 %1, i32* @b, align 4

  ret void
  ; ELIM-NEXT: ret void
}
