; RUN: llc -mtriple=i686-unknown-nacl -O0 -filetype=asm %s -o - | FileCheck %s \
; RUN:   --check-prefix=NACL32
; RUN: llc -mtriple=i686-unknown-nacl -filetype=asm %s -o - | FileCheck %s \
; RUN:   --check-prefix=NACL32
; RUN: llc -mtriple=x86_64-unknown-nacl -O0 -filetype=asm %s -o - | \
; RUN:   FileCheck %s --check-prefix=NACL64
; RUN: llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -o - | \
; RUN:   FileCheck %s --check-prefix=NACL64

; Test that various bit manipulation intrinsics are supported by the
; NaCl X86-32 and X86-64 backends.

declare i16 @llvm.bswap.i16(i16)
declare i32 @llvm.bswap.i32(i32)
declare i64 @llvm.bswap.i64(i64)

; NACL32: test_bswap_16
; NACL32: rolw $8, %{{.*}}
; NACL64: test_bswap_16
; NACL64: rolw $8, %{{.*}}
define i16 @test_bswap_16(i16 %a) {
  %b = call i16 @llvm.bswap.i16(i16 %a)
  ret i16 %b
}

; NACL32: test_bswap_const_16
; NACL32: movw $-12885, %ax # imm = 0xFFFFFFFFFFFFCDAB
; NACL64: test_bswap_const_16
; NACL64: movw $-12885, %ax # imm = 0xFFFFFFFFFFFFCDAB
define i16 @test_bswap_const_16() {
  ; 0xabcd
  %a = call i16 @llvm.bswap.i16(i16 43981)
  ret i16 %a
}

; NACL32: test_bswap_32
; NACL32: bswapl %eax
; NACL64: test_bswap_32
; NACL64: bswapl %edi
define i32 @test_bswap_32(i32 %a) {
  %b = call i32 @llvm.bswap.i32(i32 %a)
  ret i32 %b
}

; NACL32: test_bswap_const_32
; NACL32: movl $32492971, %eax # imm = 0x1EFCDAB
; NACL64: test_bswap_const_32
; NACL64: movl $32492971, %eax # imm = 0x1EFCDAB
define i32 @test_bswap_const_32() {
  ; 0xabcdef01
  %a = call i32 @llvm.bswap.i32(i32 2882400001)
  ret i32 %a
}

; NACL32: test_bswap_64
; NACL32: bswapl %e{{.*}}
; NACL32: bswapl %e{{.*}}
; NACL64: test_bswap_64
; NACL64: bswapq %rdi
define i64 @test_bswap_64(i64 %a) {
  %b = call i64 @llvm.bswap.i64(i64 %a)
  ret i64 %b
}

; NACL32: test_bswap_const_64
; NACL32: movl $32492971, %eax # imm = 0x1EFCDAB
; NACL32: movl $-1989720797, %edx # imm = 0xFFFFFFFF89674523
; NACL64: test_bswap_const_64
; NACL64: movabsq	$-8545785751253561941, %rax # imm = 0x8967452301EFCDAB
define i64 @test_bswap_const_64(i64 %a) {
  ; 0xabcdef01 23456789
  %b = call i64 @llvm.bswap.i64(i64 12379813738877118345)
  ret i64 %b
}
