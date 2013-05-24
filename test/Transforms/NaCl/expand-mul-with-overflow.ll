; RUN: opt %s -expand-mul-with-overflow -S | FileCheck %s

declare {i32, i1} @llvm.umul.with.overflow.i32(i32, i32)
declare {i64, i1} @llvm.umul.with.overflow.i64(i64, i64)

; CHECK-NOT: @llvm.umul.with.overflow


define void @umul32_by_const(i32 %x, i32* %result_val, i1* %result_overflow) {
  %pair = call {i32, i1} @llvm.umul.with.overflow.i32(i32 %x, i32 256)
  %val = extractvalue {i32, i1} %pair, 0
  %overflow = extractvalue {i32, i1} %pair, 1

  store i32 %val, i32* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}

; The bound is 16777215 == 0xffffff == ((1 << 32) - 1) / 256
; CHECK: define void @umul32_by_const(
; CHECK-NEXT: %pair.mul = mul i32 %x, 256
; CHECK-NEXT: %pair.overflow = icmp ugt i32 %x, 16777215
; CHECK-NEXT: store i32 %pair.mul, i32* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


; Check that the pass can expand multiple uses of the same intrinsic.
define void @umul32_by_const2(i32 %x, i32* %result_val, i1* %result_overflow) {
  %pair = call {i32, i1} @llvm.umul.with.overflow.i32(i32 %x, i32 65536)
  %val = extractvalue {i32, i1} %pair, 0
  ; Check that the pass can expand multiple uses of %pair.
  %overflow1 = extractvalue {i32, i1} %pair, 1
  %overflow2 = extractvalue {i32, i1} %pair, 1

  store i32 %val, i32* %result_val
  store i1 %overflow1, i1* %result_overflow
  store i1 %overflow2, i1* %result_overflow
  ret void
}

; CHECK: define void @umul32_by_const2(
; CHECK-NEXT: %pair.mul = mul i32 %x, 65536
; CHECK-NEXT: %pair.overflow = icmp ugt i32 %x, 65535
; CHECK-NEXT: store i32 %pair.mul, i32* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


define void @umul64_by_const(i64 %x, i64* %result_val, i1* %result_overflow) {
  ; Multiply by 1 << 55.
  %pair = call {i64, i1} @llvm.umul.with.overflow.i64(i64 36028797018963968, i64 %x)
  %val = extractvalue {i64, i1} %pair, 0
  %overflow = extractvalue {i64, i1} %pair, 1

  store i64 %val, i64* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}

; CHECK: define void @umul64_by_const(i64 %x, i64* %result_val, i1* %result_overflow) {
; CHECK-NEXT: %pair.mul = mul i64 %x, 36028797018963968
; CHECK-NEXT: %pair.overflow = icmp ugt i64 %x, 511
; CHECK-NEXT: store i64 %pair.mul, i64* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow
