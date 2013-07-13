; RUN: opt -nacl-rewrite-atomics -S < %s | FileCheck %s

; Each of these tests validates that the corresponding legacy GCC-style
; builtins are properly rewritten to NaCl atomic builtins. Only the
; GCC-style builtins that have corresponding primitives in C11/C++11 and
; which emit different code are tested. These legacy GCC-builtins only
; support sequential-consistency.
;
; test_* tests the corresponding __sync_* builtin. See:
; http://gcc.gnu.org/onlinedocs/gcc-4.8.1/gcc/_005f_005fsync-Builtins.html
;
; There are also tests which validate that volatile loads/stores get
; rewritten into NaCl atomic builtins. The memory ordering for volatile
; loads/stores is not validated: it could technically be constrained to
; sequential consistency, or left as relaxed.
;
; Alignment is also expected to be at least natural alignment.

target datalayout = "p:32:32:32"

; CHECK: @test_fetch_and_add_i8
define zeroext i8 @test_fetch_and_add_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 1, i8* %ptr, i8 %value, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = atomicrmw add i8* %ptr, i8 %value seq_cst
  ret i8 %res
}

; CHECK: @test_fetch_and_add_i16
define zeroext i16 @test_fetch_and_add_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 1, i16* %ptr, i16 %value, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = atomicrmw add i16* %ptr, i16 %value seq_cst
  ret i16 %res
}

; CHECK: @test_fetch_and_add_i32
define i32 @test_fetch_and_add_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 %value, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = atomicrmw add i32* %ptr, i32 %value seq_cst
  ret i32 %res
}

; CHECK: @test_fetch_and_add_i64
define i64 @test_fetch_and_add_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 1, i64* %ptr, i64 %value, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = atomicrmw add i64* %ptr, i64 %value seq_cst
  ret i64 %res
}

; CHECK: @test_fetch_and_sub_i8
define zeroext i8 @test_fetch_and_sub_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 2, i8* %ptr, i8 %value, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = atomicrmw sub i8* %ptr, i8 %value seq_cst
  ret i8 %res
}

; CHECK: @test_fetch_and_sub_i16
define zeroext i16 @test_fetch_and_sub_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 2, i16* %ptr, i16 %value, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = atomicrmw sub i16* %ptr, i16 %value seq_cst
  ret i16 %res
}

; CHECK: @test_fetch_and_sub_i32
define i32 @test_fetch_and_sub_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 2, i32* %ptr, i32 %value, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = atomicrmw sub i32* %ptr, i32 %value seq_cst
  ret i32 %res
}

; CHECK: @test_fetch_and_sub_i64
define i64 @test_fetch_and_sub_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 2, i64* %ptr, i64 %value, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = atomicrmw sub i64* %ptr, i64 %value seq_cst
  ret i64 %res
}

; CHECK: @test_fetch_and_or_i8
define zeroext i8 @test_fetch_and_or_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 3, i8* %ptr, i8 %value, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = atomicrmw or i8* %ptr, i8 %value seq_cst
  ret i8 %res
}

; CHECK: @test_fetch_and_or_i16
define zeroext i16 @test_fetch_and_or_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 3, i16* %ptr, i16 %value, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = atomicrmw or i16* %ptr, i16 %value seq_cst
  ret i16 %res
}

; CHECK: @test_fetch_and_or_i32
define i32 @test_fetch_and_or_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 3, i32* %ptr, i32 %value, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = atomicrmw or i32* %ptr, i32 %value seq_cst
  ret i32 %res
}

; CHECK: @test_fetch_and_or_i64
define i64 @test_fetch_and_or_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 3, i64* %ptr, i64 %value, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = atomicrmw or i64* %ptr, i64 %value seq_cst
  ret i64 %res
}

; CHECK: @test_fetch_and_and_i8
define zeroext i8 @test_fetch_and_and_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 4, i8* %ptr, i8 %value, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = atomicrmw and i8* %ptr, i8 %value seq_cst
  ret i8 %res
}

; CHECK: @test_fetch_and_and_i16
define zeroext i16 @test_fetch_and_and_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 4, i16* %ptr, i16 %value, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = atomicrmw and i16* %ptr, i16 %value seq_cst
  ret i16 %res
}

; CHECK: @test_fetch_and_and_i32
define i32 @test_fetch_and_and_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 4, i32* %ptr, i32 %value, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = atomicrmw and i32* %ptr, i32 %value seq_cst
  ret i32 %res
}

; CHECK: @test_fetch_and_and_i64
define i64 @test_fetch_and_and_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 4, i64* %ptr, i64 %value, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = atomicrmw and i64* %ptr, i64 %value seq_cst
  ret i64 %res
}

; CHECK: @test_fetch_and_xor_i8
define zeroext i8 @test_fetch_and_xor_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 5, i8* %ptr, i8 %value, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = atomicrmw xor i8* %ptr, i8 %value seq_cst
  ret i8 %res
}

; CHECK: @test_fetch_and_xor_i16
define zeroext i16 @test_fetch_and_xor_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 5, i16* %ptr, i16 %value, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = atomicrmw xor i16* %ptr, i16 %value seq_cst
  ret i16 %res
}

; CHECK: @test_fetch_and_xor_i32
define i32 @test_fetch_and_xor_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 5, i32* %ptr, i32 %value, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = atomicrmw xor i32* %ptr, i32 %value seq_cst
  ret i32 %res
}

; CHECK: @test_fetch_and_xor_i64
define i64 @test_fetch_and_xor_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 5, i64* %ptr, i64 %value, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = atomicrmw xor i64* %ptr, i64 %value seq_cst
  ret i64 %res
}

; CHECK: @test_val_compare_and_swap_i8
define zeroext i8 @test_val_compare_and_swap_i8(i8* %ptr, i8 zeroext %oldval, i8 zeroext %newval) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.cmpxchg.i8(i8* %ptr, i8 %oldval, i8 %newval, i32 6, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = cmpxchg i8* %ptr, i8 %oldval, i8 %newval seq_cst
  ret i8 %res
}

; CHECK: @test_val_compare_and_swap_i16
define zeroext i16 @test_val_compare_and_swap_i16(i16* %ptr, i16 zeroext %oldval, i16 zeroext %newval) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.cmpxchg.i16(i16* %ptr, i16 %oldval, i16 %newval, i32 6, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = cmpxchg i16* %ptr, i16 %oldval, i16 %newval seq_cst
  ret i16 %res
}

; CHECK: @test_val_compare_and_swap_i32
define i32 @test_val_compare_and_swap_i32(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst
  ret i32 %res
}

; CHECK: @test_val_compare_and_swap_i64
define i64 @test_val_compare_and_swap_i64(i64* %ptr, i64 %oldval, i64 %newval) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.cmpxchg.i64(i64* %ptr, i64 %oldval, i64 %newval, i32 6, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = cmpxchg i64* %ptr, i64 %oldval, i64 %newval seq_cst
  ret i64 %res
}

; CHECK: @test_synchronize
define void @test_synchronize() {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.fence(i32 6)
  ; CHECK-NEXT: ret void
  fence seq_cst
  ret void
}

; CHECK: @test_lock_test_and_set_i8
define zeroext i8 @test_lock_test_and_set_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 6, i8* %ptr, i8 %value, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = atomicrmw xchg i8* %ptr, i8 %value seq_cst
  ret i8 %res
}

; CHECK: @test_lock_release_i8
define void @test_lock_release_i8(i8* %ptr) {
  ; Note that the 'release' was changed to a 'seq_cst'.
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i8(i8 0, i8* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store atomic i8 0, i8* %ptr release, align 1
  ret void
}

; CHECK: @test_lock_test_and_set_i16
define zeroext i16 @test_lock_test_and_set_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 6, i16* %ptr, i16 %value, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = atomicrmw xchg i16* %ptr, i16 %value seq_cst
  ret i16 %res
}

; CHECK: @test_lock_release_i16
define void @test_lock_release_i16(i16* %ptr) {
  ; Note that the 'release' was changed to a 'seq_cst'.
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i16(i16 0, i16* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store atomic i16 0, i16* %ptr release, align 2
  ret void
}

; CHECK: @test_lock_test_and_set_i32
define i32 @test_lock_test_and_set_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 6, i32* %ptr, i32 %value, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = atomicrmw xchg i32* %ptr, i32 %value seq_cst
  ret i32 %res
}

; CHECK: @test_lock_release_i32
define void @test_lock_release_i32(i32* %ptr) {
  ; Note that the 'release' was changed to a 'seq_cst'.
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 0, i32* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store atomic i32 0, i32* %ptr release, align 4
  ret void
}

; CHECK: @test_lock_test_and_set_i64
define i64 @test_lock_test_and_set_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 6, i64* %ptr, i64 %value, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = atomicrmw xchg i64* %ptr, i64 %value seq_cst
  ret i64 %res
}

; CHECK: @test_lock_release_i64
define void @test_lock_release_i64(i64* %ptr) {
  ; Note that the 'release' was changed to a 'seq_cst'.
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 0, i64* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store atomic i64 0, i64* %ptr release, align 8
  ret void
}

; CHECK: @test_volatile_load_i8
define zeroext i8 @test_volatile_load_i8(i8* %ptr) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.load.i8(i8* %ptr, i32 6)
  ; CHECK-NEXT: ret i8 %res
  %res = load volatile i8* %ptr, align 1
  ret i8 %res
}

; CHECK: @test_volatile_store_i8
define void @test_volatile_store_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i8(i8 %value, i8* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store volatile i8 %value, i8* %ptr, align 1
  ret void
}

; CHECK: @test_volatile_load_i16
define zeroext i16 @test_volatile_load_i16(i16* %ptr) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.load.i16(i16* %ptr, i32 6)
  ; CHECK-NEXT: ret i16 %res
  %res = load volatile i16* %ptr, align 2
  ret i16 %res
}

; CHECK: @test_volatile_store_i16
define void @test_volatile_store_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i16(i16 %value, i16* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store volatile i16 %value, i16* %ptr, align 2
  ret void
}

; CHECK: @test_volatile_load_i32
define i32 @test_volatile_load_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  ; CHECK-NEXT: ret i32 %res
  %res = load volatile i32* %ptr, align 4
  ret i32 %res
}

; CHECK: @test_volatile_store_i32
define void @test_volatile_store_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store volatile i32 %value, i32* %ptr, align 4
  ret void
}

; CHECK: @test_volatile_load_i64
define i64 @test_volatile_load_i64(i64* %ptr) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr, i32 6)
  ; CHECK-NEXT: ret i64 %res
  %res = load volatile i64* %ptr, align 8
  ret i64 %res
}

; CHECK: @test_volatile_store_i64
define void @test_volatile_store_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value, i64* %ptr, i32 6)
  ; CHECK-NEXT: ret void
  store volatile i64 %value, i64* %ptr, align 8
  ret void
}

; CHECK: @test_volatile_load_float
define float @test_volatile_load_float(float* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast float* %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i32 %res to float
  ; CHECK-NEXT: ret float %res.cast
  %res = load volatile float* %ptr, align 4
  ret float %res
}

; CHECK: @test_volatile_store_float
define void @test_volatile_store_float(float* %ptr, float %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast float* %ptr to i32*
  ; CHECK-NEXT: %value.cast = bitcast float %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: ret void
  store volatile float %value, float* %ptr, align 4
  ret void
}

; CHECK: @test_volatile_load_double
define double @test_volatile_load_double(double* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast double* %ptr to i64*
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i64 %res to double
  ; CHECK-NEXT: ret double %res.cast
  %res = load volatile double* %ptr, align 8
  ret double %res
}

; CHECK: @test_volatile_store_double
define void @test_volatile_store_double(double* %ptr, double %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast double* %ptr to i64*
  ; CHECK-NEXT: %value.cast = bitcast double %value to i64
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value.cast, i64* %ptr.cast, i32 6)
  ; CHECK-NEXT: ret void
  store volatile double %value, double* %ptr, align 8
  ret void
}

; CHECK: @test_volatile_load_i32_pointer
define i32* @test_volatile_load_i32_pointer(i32** %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast i32** %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = inttoptr i32 %res to i32*
  ; CHECK-NEXT: ret i32* %res.cast
  %res = load volatile i32** %ptr, align 4
  ret i32* %res
}

; CHECK: @test_volatile_store_i32_pointer
define void @test_volatile_store_i32_pointer(i32** %ptr, i32* %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast i32** %ptr to i32*
  ; CHECK-NEXT: %value.cast = ptrtoint i32* %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: ret void
  store volatile i32* %value, i32** %ptr, align 4
  ret void
}

; CHECK: @test_volatile_load_double_pointer
define double* @test_volatile_load_double_pointer(double** %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast double** %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = inttoptr i32 %res to double*
  ; CHECK-NEXT: ret double* %res.cast
  %res = load volatile double** %ptr, align 4
  ret double* %res
}

; CHECK: @test_volatile_store_double_pointer
define void @test_volatile_store_double_pointer(double** %ptr, double* %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast double** %ptr to i32*
  ; CHECK-NEXT: %value.cast = ptrtoint double* %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: ret void
  store volatile double* %value, double** %ptr, align 4
  ret void
}

; CHECK: @test_volatile_load_v4i8
define <4 x i8> @test_volatile_load_v4i8(<4 x i8>* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i8>* %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i32 %res to <4 x i8>
  ; CHECK-NEXT: ret <4 x i8> %res.cast
  %res = load volatile <4 x i8>* %ptr, align 8
  ret <4 x i8> %res
}

; CHECK: @test_volatile_store_v4i8
define void @test_volatile_store_v4i8(<4 x i8>* %ptr, <4 x i8> %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i8>* %ptr to i32*
  ; CHECK-NEXT: %value.cast = bitcast <4 x i8> %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: ret void
  store volatile <4 x i8> %value, <4 x i8>* %ptr, align 8
  ret void
}

; CHECK: @test_volatile_load_v4i16
define <4 x i16> @test_volatile_load_v4i16(<4 x i16>* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i16>* %ptr to i64*
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i64 %res to <4 x i16>
  ; CHECK-NEXT: ret <4 x i16> %res.cast
  %res = load volatile <4 x i16>* %ptr, align 8
  ret <4 x i16> %res
}

; CHECK: @test_volatile_store_v4i16
define void @test_volatile_store_v4i16(<4 x i16>* %ptr, <4 x i16> %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i16>* %ptr to i64*
  ; CHECK-NEXT: %value.cast = bitcast <4 x i16> %value to i64
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value.cast, i64* %ptr.cast, i32 6)
  ; CHECK-NEXT: ret void
  store volatile <4 x i16> %value, <4 x i16>* %ptr, align 8
  ret void
}
