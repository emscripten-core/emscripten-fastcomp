; RUN: opt < %s -resolve-pnacl-intrinsics -mtriple=x86_64 -S | FileCheck %s -check-prefix=CLEANED
; 'CLEANED' only needs to check a single architecture.
; RUN: opt < %s -resolve-pnacl-intrinsics -mtriple=x86_64 -S | FileCheck %s -check-prefix=X8664
; RUN: opt < %s -resolve-pnacl-intrinsics -mtriple=i386   -S | FileCheck %s -check-prefix=X8632
; RUN: opt < %s -resolve-pnacl-intrinsics -mtriple=arm    -S | FileCheck %s -check-prefix=ARM32
; RUN: opt < %s -resolve-pnacl-intrinsics -mtriple=mipsel -S | FileCheck %s -check-prefix=MIPS32
; RUN: opt < %s -resolve-pnacl-intrinsics -mtriple=asmjs  -S | FileCheck %s -check-prefix=ASMJS

; CLEANED-NOT: call {{.*}} @llvm.nacl.atomic

declare i32 @llvm.nacl.setjmp(i8*)
declare void @llvm.nacl.longjmp(i8*, i32)
declare i1 @llvm.nacl.atomic.is.lock.free(i32, i8*)

; These declarations must be here because the function pass expects
; to find them. In real life they're inserted by the translator
; before the function pass runs.
declare i32 @setjmp(i8*)
declare void @longjmp(i8*, i32)


; X8664-LABEL:  @test_is_lock_free_1(
; X8632-LABEL:  @test_is_lock_free_1(
; ARM32-LABEL:  @test_is_lock_free_1(
; MIPS32-LABEL: @test_is_lock_free_1(
; ASMJS-LABEL:  @test_is_lock_free_1(
; X8664:  ret i1 true
; X8632:  ret i1 true
; ARM32:  ret i1 true
; MIPS32: ret i1 true
; ASMJS:  ret i1 true
define i1 @test_is_lock_free_1(i8* %ptr) {
  %res = call i1 @llvm.nacl.atomic.is.lock.free(i32 1, i8* %ptr)
  ret i1 %res
}

; X8664-LABEL:  @test_is_lock_free_2(
; X8632-LABEL:  @test_is_lock_free_2(
; ARM32-LABEL:  @test_is_lock_free_2(
; MIPS32-LABEL: @test_is_lock_free_2(
; ASMJS-LABEL:  @test_is_lock_free_2(
; X8664:  ret i1 true
; X8632:  ret i1 true
; ARM32:  ret i1 true
; MIPS32: ret i1 true
; ASMJS:  ret i1 true
define i1 @test_is_lock_free_2(i16* %ptr) {
  %ptr2 = bitcast i16* %ptr to i8*
  %res = call i1 @llvm.nacl.atomic.is.lock.free(i32 2, i8* %ptr2)
  ret i1 %res
}

; X8664-LABEL:  @test_is_lock_free_4(
; X8632-LABEL:  @test_is_lock_free_4(
; ARM32-LABEL:  @test_is_lock_free_4(
; MIPS32-LABEL: @test_is_lock_free_4(
; ASMJS-LABEL:  @test_is_lock_free_4(
; X8664:  ret i1 true
; X8632:  ret i1 true
; ARM32:  ret i1 true
; MIPS32: ret i1 true
; ASMJS:  ret i1 true
define i1 @test_is_lock_free_4(i32* %ptr) {
  %ptr2 = bitcast i32* %ptr to i8*
  %res = call i1 @llvm.nacl.atomic.is.lock.free(i32 4, i8* %ptr2)
  ret i1 %res
}

; X8664-LABEL:  @test_is_lock_free_8(
; X8632-LABEL:  @test_is_lock_free_8(
; ARM32-LABEL:  @test_is_lock_free_8(
; MIPS32-LABEL: @test_is_lock_free_8(
; ASMJS-LABEL:  @test_is_lock_free_8(
; X8664:  ret i1 true
; X8632:  ret i1 true
; ARM32:  ret i1 true
; MIPS32: ret i1 false
; ASMJS:  ret i1 false
define i1 @test_is_lock_free_8(i64* %ptr) {
  %ptr2 = bitcast i64* %ptr to i8*
  %res = call i1 @llvm.nacl.atomic.is.lock.free(i32 8, i8* %ptr2)
  ret i1 %res
}

; X8664-LABEL:  @test_is_lock_free_16(
; X8632-LABEL:  @test_is_lock_free_16(
; ARM32-LABEL:  @test_is_lock_free_16(
; MIPS32-LABEL: @test_is_lock_free_16(
; ASMJS-LABEL:  @test_is_lock_free_16(
; X8664:  ret i1 false
; X8632:  ret i1 false
; ARM32:  ret i1 false
; MIPS32: ret i1 false
; ASMJS:  ret i1 false
define i1 @test_is_lock_free_16(i128* %ptr) {
  %ptr2 = bitcast i128* %ptr to i8*
  %res = call i1 @llvm.nacl.atomic.is.lock.free(i32 16, i8* %ptr2)
  ret i1 %res
}
