; RUN: opt -nacl-rewrite-atomics -S < %s | FileCheck %s
;
; Validate that atomic non-{acquire/release/acq_rel/seq_cst} loads/stores get
; rewritten into NaCl atomic builtins with sequentially consistent memory
; ordering (enum value 6), and that acquire/release/acq_rel remain as-is (enum
; values 3/4/5).
;
; Note that monotonic doesn't exist in C11/C++11, and consume isn't implemented
; in LLVM yet.

target datalayout = "p:32:32:32"

; CHECK-LABEL: @test_atomic_load_monotonic_i32
define i32 @test_atomic_load_monotonic_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  %res = load atomic i32, i32* %ptr monotonic, align 4
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_atomic_store_monotonic_i32
define void @test_atomic_store_monotonic_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr monotonic, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_unordered_i32
define i32 @test_atomic_load_unordered_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  %res = load atomic i32, i32* %ptr unordered, align 4
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_atomic_store_unordered_i32
define void @test_atomic_store_unordered_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr unordered, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_acquire_i32
define i32 @test_atomic_load_acquire_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 3)
  %res = load atomic i32, i32* %ptr acquire, align 4
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_atomic_store_release_i32
define void @test_atomic_store_release_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 4)
  store atomic i32 %value, i32* %ptr release, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_fetch_and_add_i32
define i32 @test_fetch_and_add_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 %value, i32 5)
  %res = atomicrmw add i32* %ptr, i32 %value acq_rel
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; Test all the valid cmpxchg orderings for success and failure.

; CHECK-LABEL: @test_cmpxchg_seqcst_seqcst
define { i32, i1 } @test_cmpxchg_seqcst_seqcst(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 6, i32 6)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value seq_cst seq_cst
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_seqcst_acquire
define { i32, i1 } @test_cmpxchg_seqcst_acquire(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 6, i32 3)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value seq_cst acquire
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_seqcst_relaxed
define { i32, i1 } @test_cmpxchg_seqcst_relaxed(i32* %ptr, i32 %value) {
  ; Failure ordering is upgraded.
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 6, i32 6)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value seq_cst monotonic
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_acqrel_acquire
define { i32, i1 } @test_cmpxchg_acqrel_acquire(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 5, i32 3)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value acq_rel acquire
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_acqrel_relaxed
define { i32, i1 } @test_cmpxchg_acqrel_relaxed(i32* %ptr, i32 %value) {
  ; Success and failure ordering are upgraded.
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 6, i32 6)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value acq_rel monotonic
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_release_relaxed
define { i32, i1 } @test_cmpxchg_release_relaxed(i32* %ptr, i32 %value) {
  ; Success and failure ordering are upgraded.
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 6, i32 6)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value release monotonic
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_acquire_acquire
define { i32, i1 } @test_cmpxchg_acquire_acquire(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 3, i32 3)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value acquire acquire
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_acquire_relaxed
define { i32, i1 } @test_cmpxchg_acquire_relaxed(i32* %ptr, i32 %value) {
  ; Failure ordering is upgraded.
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 3, i32 3)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value acquire monotonic
  ret { i32, i1 } %res
}

; CHECK-LABEL: @test_cmpxchg_relaxed_relaxed
define { i32, i1 } @test_cmpxchg_relaxed_relaxed(i32* %ptr, i32 %value) {
  ; Failure ordering is upgraded.
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 %value, i32 6, i32 6)
  %res = cmpxchg i32* %ptr, i32 0, i32 %value monotonic monotonic
  ret { i32, i1 } %res
}
