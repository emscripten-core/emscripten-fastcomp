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

; CHECK-LABEL: @test_fetch_and_add_i8
define zeroext i8 @test_fetch_and_add_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 1, i8* %ptr, i8 %value, i32 6)
  %res = atomicrmw add i8* %ptr, i8 %value seq_cst
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_fetch_and_add_i16
define zeroext i16 @test_fetch_and_add_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 1, i16* %ptr, i16 %value, i32 6)
  %res = atomicrmw add i16* %ptr, i16 %value seq_cst
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_fetch_and_add_i32
define i32 @test_fetch_and_add_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw add i32* %ptr, i32 %value seq_cst
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_fetch_and_add_i64
define i64 @test_fetch_and_add_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 1, i64* %ptr, i64 %value, i32 6)
  %res = atomicrmw add i64* %ptr, i64 %value seq_cst
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}

; CHECK-LABEL: @test_fetch_and_sub_i8
define zeroext i8 @test_fetch_and_sub_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 2, i8* %ptr, i8 %value, i32 6)
  %res = atomicrmw sub i8* %ptr, i8 %value seq_cst
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_fetch_and_sub_i16
define zeroext i16 @test_fetch_and_sub_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 2, i16* %ptr, i16 %value, i32 6)
  %res = atomicrmw sub i16* %ptr, i16 %value seq_cst
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_fetch_and_sub_i32
define i32 @test_fetch_and_sub_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 2, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw sub i32* %ptr, i32 %value seq_cst
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_fetch_and_sub_i64
define i64 @test_fetch_and_sub_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 2, i64* %ptr, i64 %value, i32 6)
  %res = atomicrmw sub i64* %ptr, i64 %value seq_cst
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}

; CHECK-LABEL: @test_fetch_and_or_i8
define zeroext i8 @test_fetch_and_or_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 3, i8* %ptr, i8 %value, i32 6)
  %res = atomicrmw or i8* %ptr, i8 %value seq_cst
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_fetch_and_or_i16
define zeroext i16 @test_fetch_and_or_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 3, i16* %ptr, i16 %value, i32 6)
  %res = atomicrmw or i16* %ptr, i16 %value seq_cst
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_fetch_and_or_i32
define i32 @test_fetch_and_or_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 3, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw or i32* %ptr, i32 %value seq_cst
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_fetch_and_or_i64
define i64 @test_fetch_and_or_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 3, i64* %ptr, i64 %value, i32 6)
  %res = atomicrmw or i64* %ptr, i64 %value seq_cst
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}

; CHECK-LABEL: @test_fetch_and_and_i8
define zeroext i8 @test_fetch_and_and_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 4, i8* %ptr, i8 %value, i32 6)
  %res = atomicrmw and i8* %ptr, i8 %value seq_cst
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_fetch_and_and_i16
define zeroext i16 @test_fetch_and_and_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 4, i16* %ptr, i16 %value, i32 6)
  %res = atomicrmw and i16* %ptr, i16 %value seq_cst
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_fetch_and_and_i32
define i32 @test_fetch_and_and_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 4, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw and i32* %ptr, i32 %value seq_cst
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_fetch_and_and_i64
define i64 @test_fetch_and_and_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 4, i64* %ptr, i64 %value, i32 6)
  %res = atomicrmw and i64* %ptr, i64 %value seq_cst
  ret i64 %res  ; CHECK-NEXT: ret i64 %res

}

; CHECK-LABEL: @test_fetch_and_xor_i8
define zeroext i8 @test_fetch_and_xor_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.rmw.i8(i32 5, i8* %ptr, i8 %value, i32 6)
  %res = atomicrmw xor i8* %ptr, i8 %value seq_cst
  ret i8 %res  ; CHECK-NEXT: ret i8 %res

}

; CHECK-LABEL: @test_fetch_and_xor_i16
define zeroext i16 @test_fetch_and_xor_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.rmw.i16(i32 5, i16* %ptr, i16 %value, i32 6)
  %res = atomicrmw xor i16* %ptr, i16 %value seq_cst
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_fetch_and_xor_i32
define i32 @test_fetch_and_xor_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.rmw.i32(i32 5, i32* %ptr, i32 %value, i32 6)
  %res = atomicrmw xor i32* %ptr, i32 %value seq_cst
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_fetch_and_xor_i64
define i64 @test_fetch_and_xor_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.rmw.i64(i32 5, i64* %ptr, i64 %value, i32 6)
  %res = atomicrmw xor i64* %ptr, i64 %value seq_cst
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}
