; RUN: opt %s -expand-arith-with-overflow -expand-struct-regs -S | FileCheck %s
; RUN: opt %s -expand-arith-with-overflow -expand-struct-regs -S | \
; RUN:     FileCheck %s -check-prefix=CLEANUP

declare {i8, i1} @llvm.sadd.with.overflow.i8(i8, i8)
declare {i8, i1} @llvm.ssub.with.overflow.i8(i8, i8)
declare {i16, i1} @llvm.uadd.with.overflow.i16(i16, i16)
declare {i16, i1} @llvm.usub.with.overflow.i16(i16, i16)
declare {i32, i1} @llvm.umul.with.overflow.i32(i32, i32)
declare {i64, i1} @llvm.umul.with.overflow.i64(i64, i64)
declare {i64, i1} @llvm.smul.with.overflow.i64(i64, i64)

; CLEANUP-NOT: with.overflow
; CLEANUP-NOT: extractvalue
; CLEANUP-NOT: insertvalue


define void @umul32_by_zero(i32 %x, i32* %result_val, i1* %result_overflow) {
  %pair = call {i32, i1} @llvm.umul.with.overflow.i32(i32 %x, i32 0)
  %val = extractvalue {i32, i1} %pair, 0
  %overflow = extractvalue {i32, i1} %pair, 1

  store i32 %val, i32* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; Make sure it doesn't segfault because of a division by zero.
; CHECK: define void @umul32_by_zero(
; CHECK-NEXT: %pair.arith = mul i32 %x, 0
; CHECK-NEXT: store i32 %pair.arith, i32* %result_val
; CHECK-NEXT: store i1 false, i1* %result_overflow


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
; CHECK-NEXT: %pair.arith = mul i32 %x, 256
; CHECK-NEXT: %pair.overflow = icmp ugt i32 %x, 16777215
; CHECK-NEXT: store i32 %pair.arith, i32* %result_val
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
; CHECK-NEXT: %pair.arith = mul i32 %x, 65536
; CHECK-NEXT: %pair.overflow = icmp ugt i32 %x, 65535
; CHECK-NEXT: store i32 %pair.arith, i32* %result_val
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
; CHECK: define void @umul64_by_const(
; CHECK-NEXT: %pair.arith = mul i64 36028797018963968, %x
; CHECK-NEXT: %pair.overflow = icmp ugt i64 %x, 511
; CHECK-NEXT: store i64 %pair.arith, i64* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


define void @umul64_by_var(i64 %x, i64 %y, i64* %result_val, i1* %result_overflow) {
  %pair = call {i64, i1} @llvm.umul.with.overflow.i64(i64 %x, i64 %y)
  %val = extractvalue {i64, i1} %pair, 0
  %overflow = extractvalue {i64, i1} %pair, 1

  store i64 %val, i64* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @umul64_by_var(
; CHECK-NEXT: %pair.arith = mul i64 %x, %y
; CHECK-NEXT: %pair.iszero = icmp eq i64 %y, 0
; CHECK-NEXT: %pair.denom = select i1 %pair.iszero, i64 1, i64 %y
; CHECK-NEXT: %pair.div = udiv i64 %pair.arith, %pair.denom
; CHECK-NEXT: %pair.same = icmp ne i64 %pair.div, %x
; CHECK-NEXT: %pair.overflow = select i1 %pair.iszero, i1 false, i1 %pair.same
; CHECK-NEXT: store i64 %pair.arith, i64* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


define void @smul64_by_var(i64 %x, i64 %y, i64* %result_val, i1* %result_overflow) {
  %pair = call {i64, i1} @llvm.smul.with.overflow.i64(i64 %x, i64 %y)
  %val = extractvalue {i64, i1} %pair, 0
  %overflow = extractvalue {i64, i1} %pair, 1

  store i64 %val, i64* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @smul64_by_var(
; CHECK-NEXT: %pair.arith = mul i64 %x, %y
; CHECK-NEXT: %pair.iszero = icmp eq i64 %y, 0
; CHECK-NEXT: %pair.denom = select i1 %pair.iszero, i64 1, i64 %y
; CHECK-NEXT: %pair.div = sdiv i64 %pair.arith, %pair.denom
; CHECK-NEXT: %pair.same = icmp ne i64 %pair.div, %x
; CHECK-NEXT: %pair.overflow = select i1 %pair.iszero, i1 false, i1 %pair.same
; CHECK-NEXT: store i64 %pair.arith, i64* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


define void @uadd16_with_const(i16 %x, i16* %result_val, i1* %result_overflow) {
  %pair = call {i16, i1} @llvm.uadd.with.overflow.i16(i16 %x, i16 35)
  %val = extractvalue {i16, i1} %pair, 0
  %overflow = extractvalue {i16, i1} %pair, 1

  store i16 %val, i16* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @uadd16_with_const(
; CHECK-NEXT: %pair.arith = add i16 %x, 35
; CHECK-NEXT: %pair.overflow = icmp ugt i16 %x, -36
; CHECK-NEXT: store i16 %pair.arith, i16* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


define void @uadd16_with_var(i16 %x, i16 %y, i16* %result_val, i1* %result_overflow) {
  %pair = call {i16, i1} @llvm.uadd.with.overflow.i16(i16 %x, i16 %y)
  %val = extractvalue {i16, i1} %pair, 0
  %overflow = extractvalue {i16, i1} %pair, 1

  store i16 %val, i16* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @uadd16_with_var(
; CHECK-NEXT: %pair.arith = add i16 %x, %y
; CHECK-NEXT: %pair.overflow = icmp ult i16 %pair.arith, %x
; CHECK-NEXT: store i16 %pair.arith, i16* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow

define void @usub16_with_const(i16 %x, i16* %result_val, i1* %result_overflow) {
  %pair = call {i16, i1} @llvm.usub.with.overflow.i16(i16 %x, i16 35)
  %val = extractvalue {i16, i1} %pair, 0
  %overflow = extractvalue {i16, i1} %pair, 1

  store i16 %val, i16* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @usub16_with_const(
; CHECK-NEXT: %pair.arith = sub i16 %x, 35
; CHECK-NEXT: %pair.overflow = icmp ult i16 %x, 35
; CHECK-NEXT: store i16 %pair.arith, i16* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow


define void @usub16_with_var(i16 %x, i16 %y, i16* %result_val, i1* %result_overflow) {
  %pair = call {i16, i1} @llvm.usub.with.overflow.i16(i16 %x, i16 %y)
  %val = extractvalue {i16, i1} %pair, 0
  %overflow = extractvalue {i16, i1} %pair, 1

  store i16 %val, i16* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @usub16_with_var(
; CHECK-NEXT: %pair.arith = sub i16 %x, %y
; CHECK-NEXT: %pair.overflow = icmp ult i16 %x, %y
; CHECK-NEXT: store i16 %pair.arith, i16* %result_val
; CHECK-NEXT: store i1 %pair.overflow, i1* %result_overflow

define void @sadd8_with_const(i8 %x, i8* %result_val, i1* %result_overflow) {
  %pair = call {i8, i1} @llvm.sadd.with.overflow.i8(i8 %x, i8 35)
  %val = extractvalue {i8, i1} %pair, 0
  %overflow = extractvalue {i8, i1} %pair, 1

  store i8 %val, i8* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @sadd8_with_const(
; CHECK-NEXT: %pair.arith = add i8 %x, 35
; CHECK-NEXT: %pair.postemp = add i8 %x, -128
; CHECK-NEXT: %pair.negtemp = add i8 %x, 127
; CHECK-NEXT: %pair.poscheck = icmp slt i8 %pair.arith, %pair.postemp
; CHECK-NEXT: %pair.negcheck = icmp sgt i8 %pair.arith, %pair.negtemp
; CHECK-NEXT: %pair.ispos = icmp sge i8 %x, 0
; CHECK-NEXT: %pair.select = select i1 %pair.ispos, i1 %pair.poscheck, i1 %pair.negcheck
; CHECK-NEXT: store i8 %pair.arith, i8* %result_val
; CHECK-NEXT: store i1 %pair.select, i1* %result_overflow


define void @sadd8_with_const_min(i8* %result_val, i1* %result_overflow) {
  %pair = call {i8, i1} @llvm.sadd.with.overflow.i8(i8 0, i8 -128)
  %val = extractvalue {i8, i1} %pair, 0
  %overflow = extractvalue {i8, i1} %pair, 1

  store i8 %val, i8* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @sadd8_with_const_min(
; CHECK-NEXT: store i8 -128, i8* %result_val
; CHECK-NEXT: store i1 false, i1* %result_overflow


define void @sadd8_with_var(i8 %x, i8 %y, i8* %result_val, i1* %result_overflow) {
  %pair = call {i8, i1} @llvm.sadd.with.overflow.i8(i8 %x, i8 %y)
  %val = extractvalue {i8, i1} %pair, 0
  %overflow = extractvalue {i8, i1} %pair, 1

  store i8 %val, i8* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @sadd8_with_var(
; CHECK-NEXT: %pair.arith = add i8 %x, %y
; CHECK-NEXT: %pair.postemp = add i8 %x, -128
; CHECK-NEXT: %pair.negtemp = add i8 %x, 127
; CHECK-NEXT: %pair.poscheck = icmp slt i8 %pair.arith, %pair.postemp
; CHECK-NEXT: %pair.negcheck = icmp sgt i8 %pair.arith, %pair.negtemp
; CHECK-NEXT: %pair.ispos = icmp sge i8 %x, 0
; CHECK-NEXT: %pair.select = select i1 %pair.ispos, i1 %pair.poscheck, i1 %pair.negcheck
; CHECK-NEXT: store i8 %pair.arith, i8* %result_val
; CHECK-NEXT: store i1 %pair.select, i1* %result_overflow


define void @ssub8_with_const(i8 %x, i8* %result_val, i1* %result_overflow) {
  %pair = call {i8, i1} @llvm.ssub.with.overflow.i8(i8 %x, i8 35)
  %val = extractvalue {i8, i1} %pair, 0
  %overflow = extractvalue {i8, i1} %pair, 1

  store i8 %val, i8* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @ssub8_with_const(
; CHECK-NEXT: %pair.arith = sub i8 %x, 35
; CHECK-NEXT: %pair.postemp = add i8 %x, -127
; CHECK-NEXT: %pair.negtemp = add i8 %x, -128
; CHECK-NEXT: %pair.poscheck = icmp slt i8 %pair.arith, %pair.postemp
; CHECK-NEXT: %pair.negcheck = icmp sgt i8 %pair.arith, %pair.negtemp
; CHECK-NEXT: %pair.ispos = icmp sge i8 %x, 0
; CHECK-NEXT: %pair.select = select i1 %pair.ispos, i1 %pair.poscheck, i1 %pair.negcheck
; CHECK-NEXT: store i8 %pair.arith, i8* %result_val
; CHECK-NEXT: store i1 %pair.select, i1* %result_overflow


define void @ssub8_with_const_min(i8* %result_val, i1* %result_overflow) {
  %pair = call {i8, i1} @llvm.ssub.with.overflow.i8(i8 0, i8 -128)
  %val = extractvalue {i8, i1} %pair, 0
  %overflow = extractvalue {i8, i1} %pair, 1

  store i8 %val, i8* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @ssub8_with_const_min(
; CHECK: store i1 true, i1* %result_overflow


define void @ssub8_with_var(i8 %x, i8 %y, i8* %result_val, i1* %result_overflow) {
  %pair = call {i8, i1} @llvm.ssub.with.overflow.i8(i8 %x, i8 %y)
  %val = extractvalue {i8, i1} %pair, 0
  %overflow = extractvalue {i8, i1} %pair, 1

  store i8 %val, i8* %result_val
  store i1 %overflow, i1* %result_overflow
  ret void
}
; CHECK: define void @ssub8_with_var(
; CHECK-NEXT: %pair.arith = sub i8 %x, %y
; CHECK-NEXT: %pair.postemp = add i8 %x, -127
; CHECK-NEXT: %pair.negtemp = add i8 %x, -128
; CHECK-NEXT: %pair.poscheck = icmp slt i8 %pair.arith, %pair.postemp
; CHECK-NEXT: %pair.negcheck = icmp sgt i8 %pair.arith, %pair.negtemp
; CHECK-NEXT: %pair.ispos = icmp sge i8 %x, 0
; CHECK-NEXT: %pair.select = select i1 %pair.ispos, i1 %pair.poscheck, i1 %pair.negcheck
; CHECK-NEXT: store i8 %pair.arith, i8* %result_val
; CHECK-NEXT: store i1 %pair.select, i1* %result_overflow
