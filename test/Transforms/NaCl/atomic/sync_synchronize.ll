; RUN: opt -nacl-rewrite-atomics -remove-asm-memory -S < %s | FileCheck %s

; Each of these tests validates that the corresponding legacy GCC-style builtins
; are properly rewritten to NaCl atomic builtins. Only the GCC-style builtins
; that have corresponding primitives in C11/C++11 and which emit different code
; are tested. These legacy GCC-builtins only support sequential-consistency
; (enum value 6).
;
; test_* tests the corresponding __sync_* builtin. See:
; http://gcc.gnu.org/onlinedocs/gcc-4.8.1/gcc/_005f_005fsync-Builtins.html

target datalayout = "p:32:32:32"

; This patterns gets emitted by C11/C++11 atomic thread fences.
;
; CHECK-LABEL: @test_c11_fence
define void @test_c11_fence() {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.fence(i32 6)
  fence seq_cst
  ret void  ; CHECK-NEXT: ret void
}

; This pattern gets emitted for ``__sync_synchronize`` and
; ``asm("":::"memory")`` when Clang is configured for NaCl.
;
; CHECK-LABEL: @test_synchronize
define void @test_synchronize() {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.fence.all()
  call void asm sideeffect "", "~{memory}"()
  fence seq_cst
  call void asm sideeffect "", "~{memory}"()
  ret void  ; CHECK-NEXT: ret void
}

; Make sure the above pattern is respected and not partially-matched.
;
; CHECK-LABEL: @test_synchronize_bad1
define void @test_synchronize_bad1() {
  ; CHECK-NOT: call void @llvm.nacl.atomic.fence.all()
  call void asm sideeffect "", "~{memory}"()
  fence seq_cst
  ret void
}

; CHECK-LABEL: @test_synchronize_bad2
define void @test_synchronize_bad2() {
  ; CHECK-NOT: call void @llvm.nacl.atomic.fence.all()
  fence seq_cst
  call void asm sideeffect "", "~{memory}"()
  ret void
}
