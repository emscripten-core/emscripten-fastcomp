; RUN: opt -S -expand-illegal-ints < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"

; CHECK: define i32 @add(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @i64Add(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @add(i64 %a, i64 %b) {
  %c = add i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @sub(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @i64Subtract(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @sub(i64 %a, i64 %b) {
  %c = sub i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @mul(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @__muldi3(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @mul(i64 %a, i64 %b) {
  %c = mul i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @sdiv(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @__divdi3(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @sdiv(i64 %a, i64 %b) {
  %c = sdiv i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @udiv(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @__udivdi3(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @udiv(i64 %a, i64 %b) {
  %c = udiv i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @srem(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @__remdi3(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @srem(i64 %a, i64 %b) {
  %c = srem i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @urem(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @__uremdi3(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @urem(i64 %a, i64 %b) {
  %c = urem i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @and(i32, i32, i32, i32) {
; CHECK:   %5 = and i32 %0, %2
; CHECK:   %6 = and i32 %1, %3
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @and(i64 %a, i64 %b) {
  %c = and i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @or(i32, i32, i32, i32) {
; CHECK:   %5 = or i32 %0, %2
; CHECK:   %6 = or i32 %1, %3
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @or(i64 %a, i64 %b) {
  %c = or i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @xor(i32, i32, i32, i32) {
; CHECK:   %5 = xor i32 %0, %2
; CHECK:   %6 = xor i32 %1, %3
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @xor(i64 %a, i64 %b) {
  %c = xor i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @lshr(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @bitshift64Lshr(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @lshr(i64 %a, i64 %b) {
  %c = lshr i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @ashr(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @bitshift64Ashr(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @ashr(i64 %a, i64 %b) {
  %c = ashr i64 %a, %b
  ret i64 %c
}

; CHECK: define i32 @shl(i32, i32, i32, i32) {
; CHECK:   %5 = call i32 @bitshift64Shl(i32 %0, i32 %1, i32 %2, i32 %3)
; CHECK:   %6 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %5
; CHECK: }
define i64 @shl(i64 %a, i64 %b) {
  %c = shl i64 %a, %b
  ret i64 %c
}


; CHECK: define i32 @icmp_eq(i32, i32, i32, i32) {
; CHECK:   %5 = icmp eq i32 %0, %2
; CHECK:   %6 = icmp eq i32 %1, %3
; CHECK:   %7 = and i1 %5, %6
; CHECK:   %d = zext i1 %7 to i32
; CHECK:   ret i32 %d
; CHECK: }
define i32 @icmp_eq(i64 %a, i64 %b) {
  %c = icmp eq i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: define i32 @icmp_ne(i32, i32, i32, i32) {
; CHECK:   %5 = icmp ne i32 %0, %2
; CHECK:   %6 = icmp ne i32 %1, %3
; CHECK:   %7 = or i1 %5, %6
; CHECK:   %d = zext i1 %7 to i32
; CHECK:   ret i32 %d
; CHECK: }
define i32 @icmp_ne(i64 %a, i64 %b) {
  %c = icmp ne i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: define i32 @icmp_slt(i32, i32, i32, i32) {
; CHECK:   %5 = icmp slt i32 %1, %3
; CHECK:   %6 = icmp eq i32 %1, %3
; CHECK:   %7 = icmp ult i32 %0, %2
; CHECK:   %8 = and i1 %6, %7
; CHECK:   %9 = or i1 %5, %8
; CHECK:   %d = zext i1 %9 to i32
; CHECK:   ret i32 %d
; CHECK: }
define i32 @icmp_slt(i64 %a, i64 %b) {
  %c = icmp slt i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: define i32 @icmp_ult(i32, i32, i32, i32) {
; CHECK:   %5 = icmp ult i32 %1, %3
; CHECK:   %6 = icmp eq i32 %1, %3
; CHECK:   %7 = icmp ult i32 %0, %2
; CHECK:   %8 = and i1 %6, %7
; CHECK:   %9 = or i1 %5, %8
; CHECK:   %d = zext i1 %9 to i32
; CHECK:   ret i32 %d
; CHECK: }
define i32 @icmp_ult(i64 %a, i64 %b) {
  %c = icmp ult i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: define i32 @load(i64* %a) {
; CHECK:   %1 = ptrtoint i64* %a to i32
; CHECK:   %2 = inttoptr i32 %1 to i32*
; CHECK:   %3 = load i32* %2
; CHECK:   %4 = add i32 %1, 4
; CHECK:   %5 = inttoptr i32 %4 to i32*
; CHECK:   %6 = load i32* %5
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %3
; CHECK: }
define i64 @load(i64 *%a) {
  %c = load i64* %a
  ret i64 %c
}

; CHECK: define i32 @aligned_load(i64* %a) {
; CHECK:   %1 = ptrtoint i64* %a to i32
; CHECK:   %2 = inttoptr i32 %1 to i32*
; CHECK:   %3 = load i32* %2, align 16
; CHECK:   %4 = add i32 %1, 4
; CHECK:   %5 = inttoptr i32 %4 to i32*
; CHECK:   %6 = load i32* %5, align 4
; CHECK:   call void @setHigh32(i32 %6)
; CHECK:   ret i32 %3
; CHECK: }
define i64 @aligned_load(i64 *%a) {
  %c = load i64* %a, align 16
  ret i64 %c
}

; CHECK: define void @store(i64* %a, i32, i32) {
; CHECK:   %3 = ptrtoint i64* %a to i32
; CHECK:   %4 = inttoptr i32 %3 to i32*
; CHECK:   store i32 %0, i32* %4
; CHECK:   %5 = add i32 %3, 4
; CHECK:   %6 = inttoptr i32 %5 to i32*
; CHECK:   store i32 %1, i32* %6
; CHECK:   ret void
; CHECK: }
define void @store(i64 *%a, i64 %b) {
  store i64 %b, i64* %a
  ret void
}

; CHECK: define void @aligned_store(i64* %a, i32, i32) {
; CHECK:   %3 = ptrtoint i64* %a to i32
; CHECK:   %4 = inttoptr i32 %3 to i32*
; CHECK:   store i32 %0, i32* %4, align 16
; CHECK:   %5 = add i32 %3, 4
; CHECK:   %6 = inttoptr i32 %5 to i32*
; CHECK:   store i32 %1, i32* %6, align 4
; CHECK:   ret void
; CHECK: }
define void @aligned_store(i64 *%a, i64 %b) {
  store i64 %b, i64* %a, align 16
  ret void
}

; CHECK: define i32 @call(i32, i32) {
; CHECK:   %3 = call i32 @foo(i32 %0, i32 %1)
; CHECK:   %4 = call i32 @getHigh32()
; CHECK:   call void @setHigh32(i32 %4)
; CHECK:   ret i32 %3
; CHECK: }
declare i64 @foo(i64 %arg)
define i64 @call(i64 %arg) {
  %ret = call i64 @foo(i64 %arg)
  ret i64 %ret
}

; CHECK: define i32 @trunc(i32, i32) {
; CHECK:   ret i32 %0
; CHECK: }
define i32 @trunc(i64 %x) {
  %y = trunc i64 %x to i32
  ret i32 %y
}

; CHECK: define i32 @zext(i32 %x) {
; CHECK:   call void @setHigh32(i32 0)
; CHECK:   ret i32 %x
; CHECK: }
define i64 @zext(i32 %x) {
  %y = zext i32 %x to i64
  ret i64 %y
}

; CHECK: define i32 @sext(i32 %x) {
; CHECK:   %1 = icmp slt i32 %x, 0
; CHECK:   %2 = sext i1 %1 to i32
; CHECK:   call void @setHigh32(i32 %2)
; CHECK:   ret i32 %x
; CHECK: }
define i64 @sext(i32 %x) {
  %y = sext i32 %x to i64
  ret i64 %y
}

; CHECK:      define void @unreachable_blocks(i64* %p) {
; CHECK-NEXT:   ret void
; CHECK-NEXT: }
define void @unreachable_blocks(i64* %p) {
  ret void

dead:
  %t = load i64* %p
  %s = add i64 %t, 1
  store i64 %s, i64* %p
  ret void
}

; CHECK: define i1 @slt_zero(i32 %a) {
; CHECK:   %1 = icmp slt i32 %a, 0
; CHECK:   %2 = sext i1 %1 to i32
; CHECK:   %3 = sext i1 %1 to i32
; CHECK:   %4 = sext i1 %1 to i32
; CHECK:   %5 = icmp slt i32 %4, 0
; CHECK:   ret i1 %5
; CHECK: }
define i1 @slt_zero(i32 %a) {
  %b = sext i32 %a to i128
  %c = icmp slt i128 %b, 0
  ret i1 %c
}
