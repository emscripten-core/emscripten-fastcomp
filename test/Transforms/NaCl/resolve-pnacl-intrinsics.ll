; RUN: opt < %s -resolve-pnacl-intrinsics -S | FileCheck %s \
; RUN:   -check-prefix=CLEANED
; RUN: opt < %s -resolve-pnacl-intrinsics -S | FileCheck %s

; CLEANED-NOT: call i32 @llvm.nacl.setjmp
; CLEANED-NOT: call void @llvm.nacl.longjmp
; CLEANED-NOT: call {{.*}} @llvm.nacl.atomic

declare i32 @llvm.nacl.setjmp(i8*)
declare void @llvm.nacl.longjmp(i8*, i32)

; Intrinsic name mangling is based on overloaded parameters only,
; including return type. Note that all pointers parameters are
; overloaded on type-pointed-to in Intrinsics.td, and are therefore
; mangled on the type-pointed-to only.
declare i8 @llvm.nacl.atomic.load.i8(i8*, i32)
declare i16 @llvm.nacl.atomic.load.i16(i16*, i32)
declare i32 @llvm.nacl.atomic.load.i32(i32*, i32)
declare i64 @llvm.nacl.atomic.load.i64(i64*, i32)
declare void @llvm.nacl.atomic.store.i8(i8, i8*, i32)
declare void @llvm.nacl.atomic.store.i16(i16, i16*, i32)
declare void @llvm.nacl.atomic.store.i32(i32, i32*, i32)
declare void @llvm.nacl.atomic.store.i64(i64, i64*, i32)
declare i8 @llvm.nacl.atomic.rmw.i8(i32, i8*, i8, i32)
declare i16 @llvm.nacl.atomic.rmw.i16(i32, i16*, i16, i32)
declare i32 @llvm.nacl.atomic.rmw.i32(i32, i32*, i32, i32)
declare i64 @llvm.nacl.atomic.rmw.i64(i32, i64*, i64, i32)
declare i8 @llvm.nacl.atomic.cmpxchg.i8(i8*, i8, i8, i32, i32)
declare i16 @llvm.nacl.atomic.cmpxchg.i16(i16*, i16, i16, i32, i32)
declare i32 @llvm.nacl.atomic.cmpxchg.i32(i32*, i32, i32, i32, i32)
declare i64 @llvm.nacl.atomic.cmpxchg.i64(i64*, i64, i64, i32, i32)
declare void @llvm.nacl.atomic.fence(i32)
declare void @llvm.nacl.atomic.fence.all()

; These declarations must be here because the function pass expects
; to find them. In real life they're inserted by the translator
; before the function pass runs.
declare i32 @setjmp(i8*)
declare void @longjmp(i8*, i32)

; For correctness, the resulting call must get the "returns_twice" attribute.
define i32 @call_setjmp(i8* %arg) {
  %val = call i32 @llvm.nacl.setjmp(i8* %arg)
; CHECK: %val = call i32 @setjmp(i8* %arg) [[RETURNS_TWICE:#[0-9]+]]
  ret i32 %val
}

define void @call_longjmp(i8* %arg, i32 %num) {
  call void @llvm.nacl.longjmp(i8* %arg, i32 %num)
; CHECK: call void @longjmp(i8* %arg, i32 %num){{$}}
  ret void
}

; atomics.

; CHECK-LABEL: @test_atomic_acquire
define i32 @test_atomic_acquire(i32* %ptr) {
  ; CHECK: %1 = load atomic i32, i32* %ptr acquire, align 4
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 3)
  ret i32 %1
}

; CHECK-LABEL: @test_atomic_release
define void @test_atomic_release(i32* %ptr, i32 %value) {
  ; CHECK: store atomic i32 %value, i32* %ptr release, align 4
  call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 4)
  ret void
}

; CHECK-LABEL: @test_atomic_acquire_release
define i32 @test_atomic_acquire_release(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw add i32* %ptr, i32 %value acq_rel
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 %value, i32 5)
  ret i32 %1
}

; CHECK-LABEL: @test_fetch_and_add_i32
define i32 @test_fetch_and_add_i32(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw add i32* %ptr, i32 %value seq_cst
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 %value, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_fetch_and_sub_i32
define i32 @test_fetch_and_sub_i32(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw sub i32* %ptr, i32 %value seq_cst
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 2, i32* %ptr, i32 %value, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_fetch_and_or_i32
define i32 @test_fetch_and_or_i32(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw or i32* %ptr, i32 %value seq_cst
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 3, i32* %ptr, i32 %value, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_fetch_and_and_i32
define i32 @test_fetch_and_and_i32(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw and i32* %ptr, i32 %value seq_cst
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 4, i32* %ptr, i32 %value, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_fetch_and_xor_i32
define i32 @test_fetch_and_xor_i32(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw xor i32* %ptr, i32 %value seq_cst
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 5, i32* %ptr, i32 %value, i32 6)
  ret i32 %1
}

; Test different compare-and-swap patterns that commonly occur and are a bit
; tricky because the PNaCl intrinsic only returns the value whereas the LLVM
; intrinsic also returns the success flag (equivalent to comparing the oldval
; with what was just loaded).

; CHECK-LABEL: @test_val_compare_and_swap_i32
define i32 @test_val_compare_and_swap_i32(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  ; CHECK-NEXT: %2 = extractvalue { i32, i1 } %1, 0
  ; CHECK-NEXT: ret i32 %2
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_val_compare_and_swap_i32_new
define i32 @test_val_compare_and_swap_i32_new(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  ; CHECK-NEXT: %res2 = extractvalue { i32, i1 } %1, 0
  ; CHECK-NEXT: ret i32 %res2
  %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  %success = icmp eq i32 %res, %oldval
  %res.insert.value = insertvalue { i32, i1 } undef, i32 %res, 0
  %res.insert.success = insertvalue { i32, i1 } %res.insert.value, i1 %success, 1
  %val = extractvalue { i32, i1 } %res.insert.success, 0
  ret i32 %val
}

; CHECK-LABEL: @test_bool_compare_and_swap_i32
define i1 @test_bool_compare_and_swap_i32(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  ; CHECK-NEXT: %success = extractvalue { i32, i1 } %1, 1
  ; CHECK-NEXT: ret i1 %success
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  %2 = icmp eq i32 %1, %oldval
  ret i1 %2
}

; CHECK-LABEL: @test_bool_compare_and_swap_i32_new
define i1 @test_bool_compare_and_swap_i32_new(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  ; CHECK-NEXT: %suc = extractvalue { i32, i1 } %1, 1
  ; CHECK-NEXT: ret i1 %suc
  %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  %success = icmp eq i32 %res, %oldval
  %res.insert.value = insertvalue { i32, i1 } undef, i32 %res, 0
  %res.insert.success = insertvalue { i32, i1 } %res.insert.value, i1 %success, 1
  %suc = extractvalue { i32, i1 } %res.insert.success, 1
  ret i1 %suc
}

; CHECK-LABEL: @test_bool_compare_and_swap_i32_reordered
define i1 @test_bool_compare_and_swap_i32_reordered(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  ; CHECK-NEXT: %success = extractvalue { i32, i1 } %1, 1
  ; CHECK-NEXT: ret i1 %success
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  %2 = icmp eq i32 %oldval, %1 ; Note operands are swapped from above.
  ret i1 %2
}

; CHECK-LABEL: @test_struct_compare_and_swap_i32
define { i32, i1 } @test_struct_compare_and_swap_i32(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  ; CHECK-NEXT: ret { i32, i1 } %1
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  %2 = icmp eq i32 %1, %oldval
  %3 = insertvalue { i32, i1 } undef, i32 %1, 0
  %4 = insertvalue { i32, i1 } %3, i1 %2, 1
  ret { i32, i1 } %4
}

; Test all allowed cmpxchg success/failure memory orderings.

; CHECK-LABEL: @test_cmpxchg_seqcst_seqcst
define i32 @test_cmpxchg_seqcst_seqcst(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_cmpxchg_seqcst_acquire
define i32 @test_cmpxchg_seqcst_acquire(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst acquire
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 3)
  ret i32 %1
}

; CHECK-LABEL: @test_cmpxchg_acquire_acquire
define i32 @test_cmpxchg_acquire_acquire(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK: %1 = cmpxchg i32* %ptr, i32 %oldval, i32 %newval acquire acquire
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 3)
  ret i32 %1
}

; CHECK-LABEL: @test_c11_fence
define void @test_c11_fence() {
  ; CHECK: fence seq_cst
  call void @llvm.nacl.atomic.fence(i32 6)
  ret void
}

; CHECK-LABEL: @test_synchronize
define void @test_synchronize() {
  ; CHECK: call void asm sideeffect "", "~{memory}"()
  ; CHECK: fence seq_cst
  ; CHECK: call void asm sideeffect "", "~{memory}"()
  call void @llvm.nacl.atomic.fence.all()
  ret void
}

; CHECK-LABEL: @test_lock_test_and_set_i32
define i32 @test_lock_test_and_set_i32(i32* %ptr, i32 %value) {
  ; CHECK: %1 = atomicrmw xchg i32* %ptr, i32 %value seq_cst
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 6, i32* %ptr, i32 %value, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_lock_release_i32
define void @test_lock_release_i32(i32* %ptr) {
  ; Note that the 'release' was changed to a 'seq_cst'.
  ; CHECK: store atomic i32 0, i32* %ptr seq_cst, align 4
  call void @llvm.nacl.atomic.store.i32(i32 0, i32* %ptr, i32 6)
  ret void
}

; CHECK-LABEL: @test_atomic_load_i8
define zeroext i8 @test_atomic_load_i8(i8* %ptr) {
  ; CHECK: %1 = load atomic i8, i8* %ptr seq_cst, align 1
  %1 = call i8 @llvm.nacl.atomic.load.i8(i8* %ptr, i32 6)
  ret i8 %1
}

; CHECK-LABEL: @test_atomic_store_i8
define void @test_atomic_store_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK: store atomic i8 %value, i8* %ptr seq_cst, align 1
  call void @llvm.nacl.atomic.store.i8(i8 %value, i8* %ptr, i32 6)
  ret void
}

; CHECK-LABEL: @test_atomic_load_i16
define zeroext i16 @test_atomic_load_i16(i16* %ptr) {
  ; CHECK: %1 = load atomic i16, i16* %ptr seq_cst, align 2
  %1 = call i16 @llvm.nacl.atomic.load.i16(i16* %ptr, i32 6)
  ret i16 %1
}

; CHECK-LABEL: @test_atomic_store_i16
define void @test_atomic_store_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK: store atomic i16 %value, i16* %ptr seq_cst, align 2
  call void @llvm.nacl.atomic.store.i16(i16 %value, i16* %ptr, i32 6)
  ret void
}

; CHECK-LABEL: @test_atomic_load_i32
define i32 @test_atomic_load_i32(i32* %ptr) {
  ; CHECK: %1 = load atomic i32, i32* %ptr seq_cst, align 4
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  ret i32 %1
}

; CHECK-LABEL: @test_atomic_store_i32
define void @test_atomic_store_i32(i32* %ptr, i32 %value) {
  ; CHECK: store atomic i32 %value, i32* %ptr seq_cst, align 4
  call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  ret void
}

; CHECK-LABEL: @test_atomic_load_i64
define i64 @test_atomic_load_i64(i64* %ptr) {
  ; CHECK: %1 = load atomic i64, i64* %ptr seq_cst, align 8
  %1 = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr, i32 6)
  ret i64 %1
}

; CHECK-LABEL: @test_atomic_store_i64
define void @test_atomic_store_i64(i64* %ptr, i64 %value) {
  ; CHECK: store atomic i64 %value, i64* %ptr seq_cst, align 8
  call void @llvm.nacl.atomic.store.i64(i64 %value, i64* %ptr, i32 6)
  ret void
}

; CHECK: attributes [[RETURNS_TWICE]] = { returns_twice }
