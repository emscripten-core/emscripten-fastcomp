; RUN: opt -nacl-rewrite-atomics -S < %s | FileCheck %s

; Each of these tests validates that the corresponding legacy GCC-style builtins
; are properly rewritten to NaCl atomic builtins. Only the GCC-style builtins
; that have corresponding primitives in C11/C++11 and which emit different code
; are tested. These legacy GCC-builtins only support sequential-consistency
; (enum value 6).
;
; test_* tests the corresponding __sync_* builtin. See:
; http://gcc.gnu.org/onlinedocs/gcc-4.8.1/gcc/_005f_005fsync-Builtins.html

target datalayout = "p:32:32:32"

; CHECK-LABEL: @test_lock_test_and_set_i8
define zeroext i8 @test_lock_test_and_set_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 6, i8* %ptr, i8 %value, i32 6)
  %res = atomicrmw xchg i8* %ptr, i8 %value seq_cst
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_lock_release_i8
define void @test_lock_release_i8(i8* %ptr) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i8(i8 0, i8* %ptr, i32 4)
  store atomic i8 0, i8* %ptr release, align 1
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_lock_test_and_set_i16
define zeroext i16 @test_lock_test_and_set_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 6, i16* %ptr, i16 %value, i32 6)
  %res = atomicrmw xchg i16* %ptr, i16 %value seq_cst
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_lock_release_i16
define void @test_lock_release_i16(i16* %ptr) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i16(i16 0, i16* %ptr, i32 4)
  store atomic i16 0, i16* %ptr release, align 2
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_lock_test_and_set_i32
define i32 @test_lock_test_and_set_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 6, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw xchg i32* %ptr, i32 %value seq_cst
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_lock_release_i32
define void @test_lock_release_i32(i32* %ptr) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 0, i32* %ptr, i32 4)
  store atomic i32 0, i32* %ptr release, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_lock_test_and_set_i64
define i64 @test_lock_test_and_set_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 6, i64* %ptr, i64 %value, i32 6)
  %res = atomicrmw xchg i64* %ptr, i64 %value seq_cst
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}

; CHECK-LABEL: @test_lock_release_i64
define void @test_lock_release_i64(i64* %ptr) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 0, i64* %ptr, i32 4)
  store atomic i64 0, i64* %ptr release, align 8
  ret void  ; CHECK-NEXT: ret void
}
