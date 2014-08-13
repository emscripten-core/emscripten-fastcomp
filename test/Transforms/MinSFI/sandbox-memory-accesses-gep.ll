; RUN: opt %s -expand-getelementptr -replace-ptrs-with-ints \
; RUN:        -minsfi-sandbox-memory-accesses -S \
; RUN:   | FileCheck %s -check-prefix=CHECK-GEP
; RUN: opt %s -expand-getelementptr -minsfi-sandbox-memory-accesses -S \ 
; RUN:   | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture readonly, i32, i32, i1)
declare void @llvm.memmove.p0i8.p0i8.i32(i8* nocapture, i8* nocapture readonly, i32, i32, i1)
declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1)

; This test verifies that the pass recognizes the pointer arithmetic pattern
; produced by the ExpandGetElementPtr pass and that it emits a more efficient
; address sandboxing than in the general case.

define i32 @test_load_elementptr([100 x i32]* %foo) {
  %elem = getelementptr inbounds [100 x i32]* %foo, i32 0, i32 97
  %val = load i32* %elem
  ret i32 %val
}

; CHECK-GEP-LABEL: define i32 @test_load_elementptr(i32 %foo) {
; CHECK-GEP-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-GEP-NEXT:    %1 = zext i32 %foo to i64
; CHECK-GEP-NEXT:    %2 = add i64 %mem_base, %1
; CHECK-GEP-NEXT:    %3 = add i64 %2, 388
; CHECK-GEP-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-GEP-NEXT:    %val = load i32* %4
; CHECK-GEP-NEXT:    ret i32 %val
; CHECK-GEP-NEXT:  }

define <4 x float> @test_max_offset(i32 %x) {
  %1 = add i32 %x, 1048560  ; 1MB - 16B
  %ptr = inttoptr i32 %1 to <4 x float>*
  %val = load <4 x float>* %ptr
  ret <4 x float> %val
}

; CHECK-LABEL: define <4 x float> @test_max_offset(i32 %x) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = zext i32 %x to i64
; CHECK-NEXT:    %2 = add i64 %mem_base, %1
; CHECK-NEXT:    %3 = add i64 %2, 1048560
; CHECK-NEXT:    %4 = inttoptr i64 %3 to <4 x float>*
; CHECK-NEXT:    %val = load <4 x float>* %4
; CHECK-NEXT:    ret <4 x float> %val
; CHECK-NEXT:  }

; This will not get optimized as it could access memory past the guard region.
define <4 x float> @test_offset_overflow(i32 %x) {
  %1 = add i32 %x, 1048561
  %ptr = inttoptr i32 %1 to <4 x float>*
  %val = load <4 x float>* %ptr
  ret <4 x float> %val
}

; CHECK-LABEL: define <4 x float> @test_offset_overflow(i32 %x) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = add i32 %x, 1048561
; CHECK-NEXT:    %ptr = inttoptr i32 %1 to <4 x float>*
; CHECK-NEXT:    %2 = ptrtoint <4 x float>* %ptr to i32 
; CHECK-NEXT:    %3 = zext i32 %2 to i64
; CHECK-NEXT:    %4 = add i64 %mem_base, %3
; CHECK-NEXT:    %5 = inttoptr i64 %4 to <4 x float>*
; CHECK-NEXT:    %val = load <4 x float>* %5
; CHECK-NEXT:    ret <4 x float> %val
; CHECK-NEXT:  }

define void @test_not_applied_on_memcpy(i32 %x) {
  %1 = add i32 %x, 1024
  %ptr = inttoptr i32 %1 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr, i8* %ptr, i32 2048, i32 4, i1 false);
  ret void
}

; CHECK-LABEL: define void @test_not_applied_on_memcpy(i32 %x) {
; CHECK:         [[IPTR1:%[0-9]+]] = ptrtoint i8* %ptr to i32 
; CHECK-NEXT:    [[ZEXT1:%[0-9]+]] = zext i32 [[IPTR1]] to i64
; CHECK-NEXT:    [[BASE1:%[0-9]+]] = add i64 %mem_base, [[ZEXT1]]
; CHECK-NEXT:    inttoptr i64 [[BASE1]] to i8*
; CHECK-NEXT:    [[IPTR2:%[0-9]+]] = ptrtoint i8* %ptr to i32 
; CHECK-NEXT:    [[ZEXT2:%[0-9]+]] = zext i32 [[IPTR2]] to i64
; CHECK-NEXT:    [[BASE2:%[0-9]+]] = add i64 %mem_base, [[ZEXT2]]
; CHECK-NEXT:    inttoptr i64 [[BASE2]] to i8*
; CHECK-NEXT:    call void @llvm.memcpy.p0i8.p0i8.i32

define void @test_not_applied_on_memmove(i32 %x) {
  %1 = add i32 %x, 1024
  %ptr = inttoptr i32 %1 to i8*
  call void @llvm.memmove.p0i8.p0i8.i32(i8* %ptr, i8* %ptr, i32 2048, i32 4, i1 false);
  ret void
}

; CHECK-LABEL: define void @test_not_applied_on_memmove(i32 %x) {
; CHECK:         [[IPTR1:%[0-9]+]] = ptrtoint i8* %ptr to i32 
; CHECK-NEXT:    [[ZEXT1:%[0-9]+]] = zext i32 [[IPTR1]] to i64
; CHECK-NEXT:    [[BASE1:%[0-9]+]] = add i64 %mem_base, [[ZEXT1]]
; CHECK-NEXT:    inttoptr i64 [[BASE1]] to i8*
; CHECK-NEXT:    [[IPTR2:%[0-9]+]] = ptrtoint i8* %ptr to i32 
; CHECK-NEXT:    [[ZEXT2:%[0-9]+]] = zext i32 [[IPTR2]] to i64
; CHECK-NEXT:    [[BASE2:%[0-9]+]] = add i64 %mem_base, [[ZEXT2]]
; CHECK-NEXT:    inttoptr i64 [[BASE2]] to i8*
; CHECK-NEXT:    call void @llvm.memmove.p0i8.p0i8.i32

define void @test_not_applied_on_memset(i32 %x) {
  %1 = add i32 %x, 1024
  %ptr = inttoptr i32 %1 to i8*
  call void @llvm.memset.p0i8.i32(i8* %ptr, i8 3, i32 2048, i32 4, i1 false);
  ret void
}

; CHECK-LABEL: define void @test_not_applied_on_memset(i32 %x) {
; CHECK:         [[IPTR:%[0-9]+]] = ptrtoint i8* %ptr to i32 
; CHECK-NEXT:    [[ZEXT:%[0-9]+]] = zext i32 [[IPTR]] to i64
; CHECK-NEXT:    [[BASE:%[0-9]+]] = add i64 %mem_base, [[ZEXT]]
; CHECK-NEXT:    inttoptr i64 [[BASE]] to i8*
; CHECK-NEXT:    call void @llvm.memset.p0i8.i32
