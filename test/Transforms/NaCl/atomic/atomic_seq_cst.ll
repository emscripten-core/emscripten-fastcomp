; RUN: opt -nacl-rewrite-atomics -S < %s | FileCheck %s
;
; Validate that sequentially consistent atomic loads/stores get rewritten into
; NaCl atomic builtins with sequentially-consistent memory ordering (enum value
; 6).

target datalayout = "p:32:32:32"

; CHECK-LABEL: @test_atomic_load_i8
define zeroext i8 @test_atomic_load_i8(i8* %ptr) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.load.i8(i8* %ptr, i32 6)
  %res = load atomic i8, i8* %ptr seq_cst, align 1
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_atomic_store_i8
define void @test_atomic_store_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i8(i8 %value, i8* %ptr, i32 6)
  store atomic i8 %value, i8* %ptr seq_cst, align 1
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_i16
define zeroext i16 @test_atomic_load_i16(i16* %ptr) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.load.i16(i16* %ptr, i32 6)
  %res = load atomic i16, i16* %ptr seq_cst, align 2
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_atomic_store_i16
define void @test_atomic_store_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i16(i16 %value, i16* %ptr, i32 6)
  store atomic i16 %value, i16* %ptr seq_cst, align 2
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_i32
define i32 @test_atomic_load_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  %res = load atomic i32, i32* %ptr seq_cst, align 4
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_atomic_store_i32
define void @test_atomic_store_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store atomic i32 %value, i32* %ptr seq_cst, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_i64
define i64 @test_atomic_load_i64(i64* %ptr) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr, i32 6)
  %res = load atomic i64, i64* %ptr seq_cst, align 8
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}

; CHECK-LABEL: @test_atomic_store_i64
define void @test_atomic_store_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value, i64* %ptr, i32 6)
  store atomic i64 %value, i64* %ptr seq_cst, align 8
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_i32_pointer
define i32* @test_atomic_load_i32_pointer(i32** %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast i32** %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = inttoptr i32 %res to i32*
  %res = load atomic i32*, i32** %ptr seq_cst, align 4
  ret i32* %res  ; CHECK-NEXT: ret i32* %res.cast
}

; CHECK-LABEL: @test_atomic_store_i32_pointer
define void @test_atomic_store_i32_pointer(i32** %ptr, i32* %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast i32** %ptr to i32*
  ; CHECK-NEXT: %value.cast = ptrtoint i32* %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  store atomic i32* %value, i32** %ptr seq_cst, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_atomic_load_double_pointer
define double* @test_atomic_load_double_pointer(double** %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast double** %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = inttoptr i32 %res to double*
  %res = load atomic double*, double** %ptr seq_cst, align 4
  ret double* %res  ; CHECK-NEXT: ret double* %res.cast
}

; CHECK-LABEL: @test_atomic_store_double_pointer
define void @test_atomic_store_double_pointer(double** %ptr, double* %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast double** %ptr to i32*
  ; CHECK-NEXT: %value.cast = ptrtoint double* %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  store atomic double* %value, double** %ptr seq_cst, align 4
  ret void  ; CHECK-NEXT: ret void
}
