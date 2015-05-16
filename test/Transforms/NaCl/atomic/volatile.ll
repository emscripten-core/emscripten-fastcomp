; RUN: opt -nacl-rewrite-atomics -S < %s | FileCheck %s
;
; Validate that volatile loads/stores get rewritten into NaCl atomic builtins.
; The memory ordering for volatile loads/stores could technically be constrained
; to sequential consistency (enum value 6), or left as relaxed.

target datalayout = "p:32:32:32"

; CHECK-LABEL: @test_volatile_load_i8
define zeroext i8 @test_volatile_load_i8(i8* %ptr) {
  ; CHECK-NEXT: %res = call i8 @llvm.nacl.atomic.load.i8(i8* %ptr, i32 6)
  %res = load volatile i8, i8* %ptr, align 1
  ret i8 %res  ; CHECK-NEXT: ret i8 %res
}

; CHECK-LABEL: @test_volatile_store_i8
define void @test_volatile_store_i8(i8* %ptr, i8 zeroext %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i8(i8 %value, i8* %ptr, i32 6)
  store volatile i8 %value, i8* %ptr, align 1
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_i16
define zeroext i16 @test_volatile_load_i16(i16* %ptr) {
  ; CHECK-NEXT: %res = call i16 @llvm.nacl.atomic.load.i16(i16* %ptr, i32 6)
  %res = load volatile i16, i16* %ptr, align 2
  ret i16 %res  ; CHECK-NEXT: ret i16 %res
}

; CHECK-LABEL: @test_volatile_store_i16
define void @test_volatile_store_i16(i16* %ptr, i16 zeroext %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i16(i16 %value, i16* %ptr, i32 6)
  store volatile i16 %value, i16* %ptr, align 2
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_i32
define i32 @test_volatile_load_i32(i32* %ptr) {
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 6)
  %res = load volatile i32, i32* %ptr, align 4
  ret i32 %res  ; CHECK-NEXT: ret i32 %res
}

; CHECK-LABEL: @test_volatile_store_i32
define void @test_volatile_store_i32(i32* %ptr, i32 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value, i32* %ptr, i32 6)
  store volatile i32 %value, i32* %ptr, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_i64
define i64 @test_volatile_load_i64(i64* %ptr) {
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr, i32 6)
  %res = load volatile i64, i64* %ptr, align 8
  ret i64 %res  ; CHECK-NEXT: ret i64 %res
}

; CHECK-LABEL: @test_volatile_store_i64
define void @test_volatile_store_i64(i64* %ptr, i64 %value) {
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value, i64* %ptr, i32 6)
  store volatile i64 %value, i64* %ptr, align 8
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_float
define float @test_volatile_load_float(float* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast float* %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i32 %res to float
  %res = load volatile float, float* %ptr, align 4
  ret float %res  ; CHECK-NEXT: ret float %res.cast
}

; CHECK-LABEL: @test_volatile_store_float
define void @test_volatile_store_float(float* %ptr, float %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast float* %ptr to i32*
  ; CHECK-NEXT: %value.cast = bitcast float %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  store volatile float %value, float* %ptr, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_double
define double @test_volatile_load_double(double* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast double* %ptr to i64*
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i64 %res to double
  %res = load volatile double, double* %ptr, align 8
  ret double %res  ; CHECK-NEXT: ret double %res.cast
}

; CHECK-LABEL: @test_volatile_store_double
define void @test_volatile_store_double(double* %ptr, double %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast double* %ptr to i64*
  ; CHECK-NEXT: %value.cast = bitcast double %value to i64
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value.cast, i64* %ptr.cast, i32 6)
  store volatile double %value, double* %ptr, align 8
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_i32_pointer
define i32* @test_volatile_load_i32_pointer(i32** %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast i32** %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = inttoptr i32 %res to i32*
  %res = load volatile i32*, i32** %ptr, align 4
  ret i32* %res  ; CHECK-NEXT: ret i32* %res.cast
}

; CHECK-LABEL: @test_volatile_store_i32_pointer
define void @test_volatile_store_i32_pointer(i32** %ptr, i32* %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast i32** %ptr to i32*
  ; CHECK-NEXT: %value.cast = ptrtoint i32* %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  store volatile i32* %value, i32** %ptr, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_double_pointer
define double* @test_volatile_load_double_pointer(double** %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast double** %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = inttoptr i32 %res to double*
  %res = load volatile double*, double** %ptr, align 4
  ret double* %res  ; CHECK-NEXT: ret double* %res.cast
}

; CHECK-LABEL: @test_volatile_store_double_pointer
define void @test_volatile_store_double_pointer(double** %ptr, double* %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast double** %ptr to i32*
  ; CHECK-NEXT: %value.cast = ptrtoint double* %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  store volatile double* %value, double** %ptr, align 4
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_v4i8
define <4 x i8> @test_volatile_load_v4i8(<4 x i8>* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i8>* %ptr to i32*
  ; CHECK-NEXT: %res = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i32 %res to <4 x i8>
  %res = load volatile <4 x i8>, <4 x i8>* %ptr, align 8
  ret <4 x i8> %res  ; CHECK-NEXT: ret <4 x i8> %res.cast
}

; CHECK-LABEL: @test_volatile_store_v4i8
define void @test_volatile_store_v4i8(<4 x i8>* %ptr, <4 x i8> %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i8>* %ptr to i32*
  ; CHECK-NEXT: %value.cast = bitcast <4 x i8> %value to i32
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i32(i32 %value.cast, i32* %ptr.cast, i32 6)
  store volatile <4 x i8> %value, <4 x i8>* %ptr, align 8
  ret void  ; CHECK-NEXT: ret void
}

; CHECK-LABEL: @test_volatile_load_v4i16
define <4 x i16> @test_volatile_load_v4i16(<4 x i16>* %ptr) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i16>* %ptr to i64*
  ; CHECK-NEXT: %res = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr.cast, i32 6)
  ; CHECK-NEXT: %res.cast = bitcast i64 %res to <4 x i16>
  %res = load volatile <4 x i16>, <4 x i16>* %ptr, align 8
  ret <4 x i16> %res  ; CHECK-NEXT: ret <4 x i16> %res.cast
}

; CHECK-LABEL: @test_volatile_store_v4i16
define void @test_volatile_store_v4i16(<4 x i16>* %ptr, <4 x i16> %value) {
  ; CHECK-NEXT: %ptr.cast = bitcast <4 x i16>* %ptr to i64*
  ; CHECK-NEXT: %value.cast = bitcast <4 x i16> %value to i64
  ; CHECK-NEXT: call void @llvm.nacl.atomic.store.i64(i64 %value.cast, i64* %ptr.cast, i32 6)
  store volatile <4 x i16> %value, <4 x i16>* %ptr, align 8
  ret void  ; CHECK-NEXT: ret void
}
