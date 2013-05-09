; RUN: llc -mtriple=armv7-unknown-nacl -filetype=asm %s -o - | FileCheck %s
; RUN: llc -mtriple=armv7-unknown-nacl -O0 -filetype=asm %s -o - | FileCheck %s

; Test that various bit manipulation intrinsics are supported by the
; NaCl ARM backend.

declare i16 @llvm.bswap.i16(i16)
declare i32 @llvm.bswap.i32(i32)
declare i64 @llvm.bswap.i64(i64)

; CHECK: test_bswap_16
; CHECK: rev [[REG:r[0-9]+]], {{r[0-9]+}}
; CHECK-NEXT: lsr {{.*}}[[REG]], {{.*}}[[REG]], #16
define i16 @test_bswap_16(i16 %a) {
  %b = call i16 @llvm.bswap.i16(i16 %a)
  ret i16 %b
}

; CHECK: test_bswap_const_16
; 0xcdab
; CHECK: movw r0, #52651
define i16 @test_bswap_const_16() {
  ; 0xabcd
  %a = call i16 @llvm.bswap.i16(i16 43981)
  ret i16 %a
}

; CHECK: test_bswap_32
; CHECK: rev [[REG:r[0-9]+]], {{r[0-9]+}}
define i32 @test_bswap_32(i32 %a) {
  %b = call i32 @llvm.bswap.i32(i32 %a)
  ret i32 %b
}

; CHECK: test_bswap_const_32
; 0x01ef cdab
; CHECK: movw r0, #52651
; CHECK: movt r0, #495
define i32 @test_bswap_const_32() {
  ; 0xabcdef01
  %a = call i32 @llvm.bswap.i32(i32 2882400001)
  ret i32 %a
}

; CHECK: test_bswap_64
; CHECK: rev [[REG1:r[0-9]+]], {{r[0-9]+}}
; CHECK: rev {{r[0-9]+}}, {{r[0-9]+}}
; CHECK: mov r0, {{.*}}[[REG1]]
define i64 @test_bswap_64(i64 %a) {
  %b = call i64 @llvm.bswap.i64(i64 %a)
  ret i64 %b
}

; CHECK: test_bswap_const_64
; 0x8967 4523 01ef cdab
; Just checking movw, since O0 and O2 have different schedules for the
; movw/movt of r0/r1.
; CHECK: movw r0, #52651
; CHECK: movw r1, #17699
define i64 @test_bswap_const_64() {
  ; 0xabcdef01 23456789
  %a = call i64 @llvm.bswap.i64(i64 12379813738877118345)
  ret i64 %a
}
