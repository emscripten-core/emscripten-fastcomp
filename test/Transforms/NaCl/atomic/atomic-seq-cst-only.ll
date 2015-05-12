; RUN: opt -nacl-rewrite-atomics -pnacl-memory-order-seq-cst-only=true -S < %s | FileCheck %s
;
; Verify that -pnacl-memory-order-seq-cst-only=true ensures all atomic memory
; orderings become seq_cst (enum value 6).
;
; Note that monotonic doesn't exist in C11/C++11, and consume isn't implemented
; in LLVM yet.

target datalayout = "p:32:32:32"

; CHECK-LABEL: @test_atomic_store_monotonic_i32
define void @test_atomic_store_monotonic_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr monotonic, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_store_unordered_i32
define void @test_atomic_store_unordered_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr unordered, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_acquire_i32
define i32 @test_atomic_load_acquire_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  %res = load atomic i32, i32* %ptr acquire, align 4
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_atomic_store_release_i32
define void @test_atomic_store_release_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr release, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_fetch_and_add_i32
define i32 @test_fetch_and_add_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw add i32* %ptr, i32 %value acq_rel
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_atomic_store_seq_cst_i32
define void @test_atomic_store_seq_cst_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr seq_cst, align 4
  ret void  ; CHECK-NEXT: ret void
}
