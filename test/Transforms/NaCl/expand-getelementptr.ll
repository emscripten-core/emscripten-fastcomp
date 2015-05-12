; RUN: opt < %s -expand-getelementptr -S | FileCheck %s

target datalayout = "p:32:32:32"

%MyStruct = type { i8, i32, i8 }
%MyArray = type { [100 x i64] }
%MyArrayOneByte = type { [100 x i8] }


; Test indexing struct field
define i8* @test_struct_field(%MyStruct* %ptr) {
  %addr = getelementptr %MyStruct, %MyStruct* %ptr, i32 0, i32 2
  ret i8* %addr
}
; CHECK: @test_struct_field
; CHECK-NEXT: %gep_int = ptrtoint %MyStruct* %ptr to i32
; CHECK-NEXT: %gep = add i32 %gep_int, 8
; CHECK-NEXT: %addr = inttoptr i32 %gep to i8*
; CHECK-NEXT: ret i8* %addr


; Test non-constant index into an array
define i64* @test_array_index(%MyArray* %ptr, i32 %index) {
  %addr = getelementptr %MyArray, %MyArray* %ptr, i32 0, i32 0, i32 %index
  ret i64* %addr
}
; CHECK: @test_array_index
; CHECK-NEXT: %gep_int = ptrtoint %MyArray* %ptr to i32
; CHECK-NEXT: %gep_array = mul i32 %index, 8
; CHECK-NEXT: %gep = add i32 %gep_int, %gep_array
; CHECK-NEXT: %addr = inttoptr i32 %gep to i64*
; CHECK-NEXT: ret i64* %addr


; Test constant index into an array (as a pointer)
define %MyStruct* @test_ptr_add(%MyStruct* %ptr) {
  %addr = getelementptr %MyStruct, %MyStruct* %ptr, i32 2
  ret %MyStruct* %addr
}
; CHECK: @test_ptr_add
; CHECK-NEXT: %gep_int = ptrtoint %MyStruct* %ptr to i32
; CHECK-NEXT: %gep = add i32 %gep_int, 24
; CHECK-NEXT: %addr = inttoptr i32 %gep to %MyStruct*
; CHECK-NEXT: ret %MyStruct* %addr


; Test that additions and multiplications are combined properly
define i64* @test_add_and_index(%MyArray* %ptr, i32 %index) {
  %addr = getelementptr %MyArray, %MyArray* %ptr, i32 1, i32 0, i32 %index
  ret i64* %addr
}
; CHECK: @test_add_and_index
; CHECK-NEXT: %gep_int = ptrtoint %MyArray* %ptr to i32
; CHECK-NEXT: %gep = add i32 %gep_int, 800
; CHECK-NEXT: %gep_array = mul i32 %index, 8
; CHECK-NEXT: %gep1 = add i32 %gep, %gep_array
; CHECK-NEXT: %addr = inttoptr i32 %gep1 to i64*
; CHECK-NEXT: ret i64* %addr


; Test that we don't multiply by 1 unnecessarily
define i8* @test_add_and_index_one_byte(%MyArrayOneByte* %ptr, i32 %index) {
  %addr = getelementptr %MyArrayOneByte, %MyArrayOneByte* %ptr, i32 1, i32 0, i32 %index
  ret i8* %addr
}
; CHECK: @test_add_and_index
; CHECK-NEXT: %gep_int = ptrtoint %MyArrayOneByte* %ptr to i32
; CHECK-NEXT: %gep = add i32 %gep_int, 100
; CHECK-NEXT: %gep1 = add i32 %gep, %index
; CHECK-NEXT: %addr = inttoptr i32 %gep1 to i8*
; CHECK-NEXT: ret i8* %addr


; Test >32-bit array index
define i64* @test_array_index64(%MyArray* %ptr, i64 %index) {
  %addr = getelementptr %MyArray, %MyArray* %ptr, i32 0, i32 0, i64 %index
  ret i64* %addr
}
; CHECK: @test_array_index64
; CHECK-NEXT: %gep_int = ptrtoint %MyArray* %ptr to i32
; CHECK-NEXT: %gep_trunc = trunc i64 %index to i32
; CHECK-NEXT: %gep_array = mul i32 %gep_trunc, 8
; CHECK-NEXT: %gep = add i32 %gep_int, %gep_array
; CHECK-NEXT: %addr = inttoptr i32 %gep to i64*
; CHECK-NEXT: ret i64* %addr


; Test <32-bit array index
define i64* @test_array_index16(%MyArray* %ptr, i16 %index) {
  %addr = getelementptr %MyArray, %MyArray* %ptr, i32 0, i32 0, i16 %index
  ret i64* %addr
}
; CHECK: @test_array_index16
; CHECK-NEXT: %gep_int = ptrtoint %MyArray* %ptr to i32
; CHECK-NEXT: %gep_sext = sext i16 %index to i32
; CHECK-NEXT: %gep_array = mul i32 %gep_sext, 8
; CHECK-NEXT: %gep = add i32 %gep_int, %gep_array
; CHECK-NEXT: %addr = inttoptr i32 %gep to i64*
; CHECK-NEXT: ret i64* %addr


; Test >32-bit constant array index
define i64* @test_array_index64_const(%MyArray* %ptr) {
  %addr = getelementptr %MyArray, %MyArray* %ptr, i32 0, i32 0, i64 100
  ret i64* %addr
}
; CHECK: @test_array_index64_const
; CHECK-NEXT: %gep_int = ptrtoint %MyArray* %ptr to i32
; CHECK-NEXT: %gep = add i32 %gep_int, 800
; CHECK-NEXT: %addr = inttoptr i32 %gep to i64*
; CHECK-NEXT: ret i64* %addr


; Test <32-bit constant array index -- test sign extension
define i64* @test_array_index16_const(%MyArray* %ptr) {
  %addr = getelementptr %MyArray, %MyArray* %ptr, i32 0, i32 0, i16 -100
  ret i64* %addr
}
; CHECK: @test_array_index16_const
; CHECK-NEXT: %gep_int = ptrtoint %MyArray* %ptr to i32
; CHECK-NEXT: %gep = add i32 %gep_int, -800
; CHECK-NEXT: %addr = inttoptr i32 %gep to i64*
; CHECK-NEXT: ret i64* %addr
