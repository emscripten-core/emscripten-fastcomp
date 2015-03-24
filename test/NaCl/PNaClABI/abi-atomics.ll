; RUN: not pnacl-abicheck < %s | FileCheck %s

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
declare i1 @llvm.nacl.atomic.is.lock.free(i32, i8*)

; This stops the verifier from complaining about the lack of an entry point.
define void @_start(i32 %arg) {
  ret void
}


; Load

define internal i32 @test_load_invalid_7() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 7)
  ret i32 %1
}
; CHECK: test_load_invalid_7 disallowed: invalid memory order

define internal i32 @test_load_invalid_0() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 0)
  ret i32 %1
}
; CHECK: test_load_invalid_0 disallowed: invalid memory order

define internal i32 @test_load_seqcst() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_load_acqrel() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 5)
  ret i32 %1
}
; CHECK: test_load_acqrel disallowed: invalid memory order

define internal i32 @test_load_release() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 4)
  ret i32 %1
}
; CHECK: test_load_release disallowed: invalid memory order

define internal i32 @test_load_acquire() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 3)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_load_consume() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 2)
  ret i32 %1
}
; CHECK: test_load_consume disallowed: invalid memory order

define internal i32 @test_load_relaxed() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 1)
  ret i32 %1
}
; CHECK: test_load_relaxed disallowed: invalid memory order


; Store

define internal void @test_store_invalid_7() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 7)
  ret void
}
; CHECK: test_store_invalid_7 disallowed: invalid memory order

define internal void @test_store_invalid_0() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 0)
  ret void
}
; CHECK: test_store_invalid_0 disallowed: invalid memory order

define internal void @test_store_seqcst() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 6)
  ret void
}
; CHECK-NOT: disallowed

define internal void @test_store_acqrel() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 5)
  ret void
}
; CHECK: test_store_acqrel disallowed: invalid memory order

define internal void @test_store_release() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 4)
  ret void
}
; CHECK-NOT: disallowed

define internal void @test_store_acquire() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 3)
  ret void
}
; CHECK: test_store_acquire disallowed: invalid memory order

define internal void @test_store_consume() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 2)
  ret void
}
; CHECK: test_store_consume disallowed: invalid memory order

define internal void @test_store_relaxed() {
  %ptr = inttoptr i32 undef to i32*
  call void @llvm.nacl.atomic.store.i32(i32 undef, i32* %ptr, i32 1)
  ret void
}
; CHECK: test_store_relaxed disallowed: invalid memory order


; rmw

define internal i32 @test_rmw_invalid_7() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 7)
  ret i32 %1
}
; CHECK: test_rmw_invalid_7 disallowed: invalid memory order

define internal i32 @test_rmw_invalid_0() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 0)
  ret i32 %1
}
; CHECK: test_rmw_invalid_0 disallowed: invalid memory order

define internal i32 @test_rmw_seqcst() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 6)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_rmw_acqrel() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 5)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_rmw_release() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 4)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_rmw_acquire() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 3)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_rmw_consume() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 2)
  ret i32 %1
}
; CHECK: test_rmw_consume disallowed: invalid memory order

define internal i32 @test_rmw_relaxed() {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 0, i32 1)
  ret i32 %1
}
; CHECK: test_rmw_relaxed disallowed: invalid memory order


; cmpxchg

define internal i32 @test_cmpxchg_invalid_7(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 7, i32 7)
  ret i32 %1
}
; CHECK: test_cmpxchg_invalid_7 disallowed: invalid memory order

define internal i32 @test_cmpxchg_invalid_0(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 0, i32 0)
  ret i32 %1
}
; CHECK: test_cmpxchg_invalid_0 disallowed: invalid memory order

; seq_cst

define internal i32 @test_cmpxchg_seqcst_seqcst(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 6)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_cmpxchg_seqcst_acqrel(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 5)
  ret i32 %1
}
; CHECK: test_cmpxchg_seqcst_acqrel disallowed: invalid memory order

define internal i32 @test_cmpxchg_seqcst_release(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 4)
  ret i32 %1
}
; CHECK: test_cmpxchg_seqcst_release disallowed: invalid memory order

define internal i32 @test_cmpxchg_seqcst_acquire(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 3)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_cmpxchg_seqcst_consume(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 2)
  ret i32 %1
}
; CHECK: test_cmpxchg_seqcst_consume disallowed: invalid memory order

define internal i32 @test_cmpxchg_seqcst_relaxed(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 6, i32 1)
  ret i32 %1
}
; CHECK: test_cmpxchg_seqcst_relaxed disallowed: invalid memory order

; acq_rel

define internal i32 @test_cmpxchg_acqrel_seqcst(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 5, i32 6)
  ret i32 %1
}
; CHECK: test_cmpxchg_acqrel_seqcst disallowed: invalid memory order

define internal i32 @test_cmpxchg_acqrel_acqrel(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 5, i32 5)
  ret i32 %1
}
; CHECK: test_cmpxchg_acqrel_acqrel disallowed: invalid memory order

define internal i32 @test_cmpxchg_acqrel_release(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 5, i32 4)
  ret i32 %1
}
; CHECK: test_cmpxchg_acqrel_release disallowed: invalid memory order

define internal i32 @test_cmpxchg_acqrel_acquire(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 5, i32 3)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_cmpxchg_acqrel_consume(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 5, i32 2)
  ret i32 %1
}
; CHECK: test_cmpxchg_acqrel_consume disallowed: invalid memory order

define internal i32 @test_cmpxchg_acqrel_relaxed(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 5, i32 1)
  ret i32 %1
}
; CHECK: test_cmpxchg_acqrel_relaxed disallowed: invalid memory order

; release

define internal i32 @test_cmpxchg_release_seqcst(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 4, i32 6)
  ret i32 %1
}
; CHECK: test_cmpxchg_release_seqcst disallowed: invalid memory order

define internal i32 @test_cmpxchg_release_acqrel(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 4, i32 5)
  ret i32 %1
}
; CHECK: test_cmpxchg_release_acqrel disallowed: invalid memory order

define internal i32 @test_cmpxchg_release_release(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 4, i32 4)
  ret i32 %1
}
; CHECK: test_cmpxchg_release_release disallowed: invalid memory order

define internal i32 @test_cmpxchg_release_acquire(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 4, i32 3)
  ret i32 %1
}
; CHECK: test_cmpxchg_release_acquire disallowed: invalid memory order

define internal i32 @test_cmpxchg_release_consume(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 4, i32 2)
  ret i32 %1
}
; CHECK: test_cmpxchg_release_consume disallowed: invalid memory order

define internal i32 @test_cmpxchg_release_relaxed(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 4, i32 1)
  ret i32 %1
}
; CHECK: test_cmpxchg_release_relaxed disallowed: invalid memory order

; acquire

define internal i32 @test_cmpxchg_acquire_seqcst(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 6)
  ret i32 %1
}
; CHECK: test_cmpxchg_acquire_seqcst disallowed: invalid memory order

define internal i32 @test_cmpxchg_acquire_acqrel(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 5)
  ret i32 %1
}
; CHECK: test_cmpxchg_acquire_acqrel disallowed: invalid memory order

define internal i32 @test_cmpxchg_acquire_release(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 4)
  ret i32 %1
}
; CHECK: test_cmpxchg_acquire_release disallowed: invalid memory order

define internal i32 @test_cmpxchg_acquire_acquire(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 3)
  ret i32 %1
}
; CHECK-NOT: disallowed

define internal i32 @test_cmpxchg_acquire_consume(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 2)
  ret i32 %1
}
; CHECK: test_cmpxchg_acquire_consume disallowed: invalid memory order

define internal i32 @test_cmpxchg_acquire_relaxed(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 3, i32 1)
  ret i32 %1
}
; CHECK: test_cmpxchg_acquire_relaxed disallowed: invalid memory order

; consume

define internal i32 @test_cmpxchg_consume_seqcst(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 2, i32 6)
  ret i32 %1
}
; CHECK: test_cmpxchg_consume_seqcst disallowed: invalid memory order

define internal i32 @test_cmpxchg_consume_acqrel(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 2, i32 5)
  ret i32 %1
}
; CHECK: test_cmpxchg_consume_acqrel disallowed: invalid memory order

define internal i32 @test_cmpxchg_consume_release(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 2, i32 4)
  ret i32 %1
}
; CHECK: test_cmpxchg_consume_release disallowed: invalid memory order

define internal i32 @test_cmpxchg_consume_acquire(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 2, i32 3)
  ret i32 %1
}
; CHECK: test_cmpxchg_consume_acquire disallowed: invalid memory order

define internal i32 @test_cmpxchg_consume_consume(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 2, i32 2)
  ret i32 %1
}
; CHECK: test_cmpxchg_consume_consume disallowed: invalid memory order

define internal i32 @test_cmpxchg_consume_relaxed(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 2, i32 1)
  ret i32 %1
}
; CHECK: test_cmpxchg_consume_relaxed disallowed: invalid memory order

; relaxed

define internal i32 @test_cmpxchg_relaxed_seqcst(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 1, i32 6)
  ret i32 %1
}
; CHECK: test_cmpxchg_relaxed_seqcst disallowed: invalid memory order

define internal i32 @test_cmpxchg_relaxed_acqrel(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 1, i32 5)
  ret i32 %1
}
; CHECK: test_cmpxchg_relaxed_acqrel disallowed: invalid memory order

define internal i32 @test_cmpxchg_relaxed_release(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 1, i32 4)
  ret i32 %1
}
; CHECK: test_cmpxchg_relaxed_release disallowed: invalid memory order

define internal i32 @test_cmpxchg_relaxed_acquire(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 1, i32 3)
  ret i32 %1
}
; CHECK: test_cmpxchg_relaxed_acquire disallowed: invalid memory order

define internal i32 @test_cmpxchg_relaxed_consume(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 1, i32 2)
  ret i32 %1
}
; CHECK: test_cmpxchg_relaxed_consume disallowed: invalid memory order

define internal i32 @test_cmpxchg_relaxed_relaxed(i32 %oldval, i32 %newval) {
  %ptr = inttoptr i32 undef to i32*
  %1 = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 %oldval, i32 %newval, i32 1, i32 1)
  ret i32 %1
}
; CHECK: test_cmpxchg_relaxed_relaxed disallowed: invalid memory order


; fence

define internal void @test_fence_invalid_7() {
  call void @llvm.nacl.atomic.fence(i32 7)
  ret void
}
; CHECK: test_fence_invalid_7 disallowed: invalid memory order

define internal void @test_fence_invalid_0() {
  call void @llvm.nacl.atomic.fence(i32 0)
  ret void
}
; CHECK: test_fence_invalid_0 disallowed: invalid memory order

define internal void @test_fence_seqcst() {
  call void @llvm.nacl.atomic.fence(i32 6)
  ret void
}
; CHECK-NOT: disallowed

define internal void @test_fence_acqrel() {
  call void @llvm.nacl.atomic.fence(i32 5)
  ret void
}
; CHECK-NOT: disallowed

define internal void @test_fence_acquire() {
  call void @llvm.nacl.atomic.fence(i32 4)
  ret void
}
; CHECK-NOT: disallowed

define internal void @test_fence_release() {
  call void @llvm.nacl.atomic.fence(i32 3)
  ret void
}
; CHECK-NOT: disallowed

define internal void @test_fence_consume() {
  call void @llvm.nacl.atomic.fence(i32 2)
  ret void
}
; CHECK: test_fence_consume disallowed: invalid memory order

define internal void @test_fence_relaxed() {
  call void @llvm.nacl.atomic.fence(i32 1)
  ret void
}
; CHECK: test_fence_relaxed disallowed: invalid memory order
