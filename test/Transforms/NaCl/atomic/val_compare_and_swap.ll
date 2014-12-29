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

; __sync_val_compare_and_swap

; CHECK-LABEL: @test_val_compare_and_swap_i8
define zeroext i8 @test_val_compare_and_swap_i8(i8* %ptr, i8 zeroext %oldval, i8 zeroext %newval) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.cmpxchg.i8(i8* %ptr, i8 %oldval, i8 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i8 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i8, i1 } undef, i8 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i8, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %val = extractvalue { i8, i1 } %res.insert.success, 0
  %res = cmpxchg i8* %ptr, i8 %oldval, i8 %newval seq_cst seq_cst
  %val = extractvalue { i8, i1 } %res, 0
  ret i8 %val  ; CHECK-NEXT: ret i8 %val
}

; CHECK-LABEL: @test_val_compare_and_swap_i16
define zeroext i16 @test_val_compare_and_swap_i16(i16* %ptr, i16 zeroext %oldval, i16 zeroext %newval) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.cmpxchg.i16(i16* %ptr, i16 %oldval, i16 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i16 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i16, i1 } undef, i16 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i16, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %val = extractvalue { i16, i1 } %res.insert.success, 0
  %res = cmpxchg i16* %ptr, i16 %oldval, i16 %newval seq_cst seq_cst
  %val = extractvalue { i16, i1 } %res, 0
  ret i16 %val  ; CHECK-NEXT: ret i16 %val
}

; CHECK-LABEL: @test_val_compare_and_swap_i32
define i32 @test_val_compare_and_swap_i32(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i32 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i32, i1 } undef, i32 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i32, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %val = extractvalue { i32, i1 } %res.insert.success, 0
  %res = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  %val = extractvalue { i32, i1 } %res, 0
  ret i32 %val  ; CHECK-NEXT: ret i32 %val
}

; CHECK-LABEL: @test_val_compare_and_swap_i64
define i64 @test_val_compare_and_swap_i64(i64* %ptr, i64 %oldval, i64 %newval) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.cmpxchg.i64(i64* %ptr, i64 %oldval, i64 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i64 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i64, i1 } undef, i64 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i64, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %val = extractvalue { i64, i1 } %res.insert.success, 0
  %res = cmpxchg i64* %ptr, i64 %oldval, i64 %newval seq_cst seq_cst
  %val = extractvalue { i64, i1 } %res, 0
  ret i64 %val  ; CHECK-NEXT: ret i64 %val
}

; __sync_bool_compare_and_swap

; CHECK-LABEL: @test_bool_compare_and_swap_i8
define zeroext i1 @test_bool_compare_and_swap_i8(i8* %ptr, i8 zeroext %oldval, i8 zeroext %newval) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.cmpxchg.i8(i8* %ptr, i8 %oldval, i8 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i8 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i8, i1 } undef, i8 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i8, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %suc = extractvalue { i8, i1 } %res.insert.success, 1
  %res = cmpxchg i8* %ptr, i8 %oldval, i8 %newval seq_cst seq_cst
  %suc = extractvalue { i8, i1 } %res, 1
  ret i1 %suc  ; CHECK-NEXT: ret i1 %suc
}

; CHECK-LABEL: @test_bool_compare_and_swap_i16
define zeroext i1 @test_bool_compare_and_swap_i16(i16* %ptr, i16 zeroext %oldval, i16 zeroext %newval) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.cmpxchg.i16(i16* %ptr, i16 %oldval, i16 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i16 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i16, i1 } undef, i16 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i16, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %suc = extractvalue { i16, i1 } %res.insert.success, 1
  %res = cmpxchg i16* %ptr, i16 %oldval, i16 %newval seq_cst seq_cst
  %suc = extractvalue { i16, i1 } %res, 1
  ret i1 %suc  ; CHECK-NEXT: ret i1 %suc
}

; CHECK-LABEL: @test_bool_compare_and_swap_i32
define i1 @test_bool_compare_and_swap_i32(i32* %ptr, i32 %oldval, i32 %newval) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i32 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i32, i1 } undef, i32 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i32, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %suc = extractvalue { i32, i1 } %res.insert.success, 1
  %res = cmpxchg i32* %ptr, i32 %oldval, i32 %newval seq_cst seq_cst
  %suc = extractvalue { i32, i1 } %res, 1
  ret i1 %suc  ; CHECK-NEXT: ret i1 %suc
}

; CHECK-LABEL: @test_bool_compare_and_swap_i64
define i1 @test_bool_compare_and_swap_i64(i64* %ptr, i64 %oldval, i64 %newval) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.cmpxchg.i64(i64* %ptr, i64 %oldval, i64 %newval, i32 6, i32 6)
  ; CHECK-NEXT: %success = icmp eq i64 %res, %oldval
  ; CHECK-NEXT: %res.insert.value = insertvalue { i64, i1 } undef, i64 %res, 0
  ; CHECK-NEXT: %res.insert.success = insertvalue { i64, i1 } %res.insert.value, i1 %success, 1
  ; CHECK-NEXT: %suc = extractvalue { i64, i1 } %res.insert.success, 1
  %res = cmpxchg i64* %ptr, i64 %oldval, i64 %newval seq_cst seq_cst
  %suc = extractvalue { i64, i1 } %res, 1
  ret i1 %suc  ; CHECK-NEXT: ret i1 %suc
}
