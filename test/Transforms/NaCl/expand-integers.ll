; RUN: opt < %s -nacl-expand-ints -S | FileCheck %s
; Test large integer expansion for operations required for large packed
; bitfields.

; CHECK-LABEL: @simpleload
define void @simpleload(i32* %a) {
; CHECK: %a96.loty = bitcast i96* %a96 to i64*
; CHECK-NEXT: %load.lo = load i64, i64* %a96.loty
; CHECK-NEXT: %a96.hi.gep = getelementptr i64, i64* %a96.loty, i32 1
; CHECK-NEXT: %a96.hity = bitcast i64* %a96.hi.gep to i32*
; CHECK-NEXT: %load.hi = load i32, i32* %a96.hity
  %a96 = bitcast i32* %a to i96*
  %load = load i96, i96* %a96

; CHECK: %a128.loty = bitcast i128* %a128 to i64*
; CHECK-NEXT: %load128.lo = load i64, i64* %a128.loty
; CHECK-NEXT: %a128.hi.gep = getelementptr i64, i64* %a128.loty, i32 1
; CHECK-NEXT: %load128.hi = load i64, i64* %a128.hi.gep
  %a128 = bitcast i32* %a to i128*
  %load128 = load i128, i128* %a128

; CHECK: %a256.loty = bitcast i256* %a256 to i64*
; CHECK-NEXT: %load256.lo = load i64, i64* %a256.loty
; CHECK-NEXT: %a256.hi.gep = getelementptr i64, i64* %a256.loty, i32 1
; CHECK-NEXT: %a256.hity = bitcast i64* %a256.hi.gep to i192*
; intermediate expansion: %load256.hi = load i192, i192* %a256.hity
; CHECK-NEXT: %a256.hity.loty = bitcast i192* %a256.hity to i64*
; CHECK-NEXT: %load256.hi.lo = load i64, i64* %a256.hity.loty
; CHECK-NEXT: %a256.hity.hi.gep = getelementptr i64, i64* %a256.hity.loty, i32 1
; CHECK-NEXT: %a256.hity.hity = bitcast i64* %a256.hity.hi.gep to i128*
; intermediate expansion: %load256.hi.hi = load i128, i128* %a256.hity.hity
; CHECK-NEXT: %a256.hity.hity.loty = bitcast i128* %a256.hity.hity to i64*
; CHECK-NEXT: %load256.hi.hi.lo = load i64, i64* %a256.hity.hity.loty
; CHECK-NEXT: %a256.hity.hity.hi.gep = getelementptr i64, i64* %a256.hity.hity.loty, i32 1
; CHECK-NEXT: %load256.hi.hi.hi = load i64, i64* %a256.hity.hity.hi.gep
  %a256 = bitcast i32* %a to i256*
  %load256 = load i256, i256* %a256
  ret void
}

; CHECK-LABEL: @loadalign
define void @loadalign(i32* %a) {
  %a96 = bitcast i32* %a to i96*

; CHECK: %load.lo = load{{.*}}, align 16
; CHECK: %load.hi = load{{.*}}, align 8
  %load = load i96, i96* %a96, align 16

; CHECK: %loadnoalign.lo = load{{.*}}, align 8
; CHECK: %loadnoalign.hi = load{{.*}}, align 8
  %loadnoalign = load i96, i96* %a96

; CHECK: %load4.lo = load{{.*}}, align 4
; CHECK: %load4.hi = load{{.*}}, align 4
  %load4 = load i96, i96* %a96, align 4

  %a256 = bitcast i32* %a to i256*
; CHECK: %load256.lo = load{{.*}}, align 16
; CHECK: %load256.hi.lo = load{{.*}}, align 8
; CHECK: %load256.hi.hi.lo = load{{.*}}, align 8
; CHECK: %load256.hi.hi.hi = load{{.*}}, align 8
  %load256 = load i256, i256* %a256, align 16
  ret void
}

; CHECK-LABEL: @simplestore
define void @simplestore(i32* %a, i32* %b) {
  %a96 = bitcast i32* %a to i96*
  %b96 = bitcast i32* %b to i96*
  %load96 = load i96, i96* %a96
; CHECK: %b96.loty = bitcast i96* %b96 to i64*
; CHECK-NEXT: store i64 %load96.lo, i64* %b96.loty
; CHECK-NEXT: %b96.hi.gep = getelementptr i64, i64* %b96.loty, i32 1
; CHECK-NEXT: %b96.hity = bitcast i64* %b96.hi.gep to i32*
; CHECK-NEXT: store i32 %load96.hi, i32* %b96.hity
  store i96 %load96, i96* %b96

  %a128 = bitcast i32* %a to i128*
  %b128 = bitcast i32* %b to i128*
  %load128 = load i128, i128* %a128
; CHECK: %b128.loty = bitcast i128* %b128 to i64*
; CHECK-NEXT: store i64 %load128.lo, i64* %b128.loty
; CHECK-NEXT: %b128.hi.gep = getelementptr i64, i64* %b128.loty, i32 1
; CHECK-NEXT: store i64 %load128.hi, i64* %b128.hi.gep
  store i128 %load128, i128* %b128

  %a256 = bitcast i32* %a to i256*
  %b256 = bitcast i32* %b to i256*
  %load256 = load i256, i256* %a256

; CHECK: %b256.loty = bitcast i256* %b256 to i64*
; CHECK-NEXT: store i64 %load256.lo, i64* %b256.loty
; CHECK-NEXT: %b256.hi.gep = getelementptr i64, i64* %b256.loty, i32 1
; CHECK-NEXT: %b256.hity = bitcast i64* %b256.hi.gep to i192*
; CHECK-NEXT: %b256.hity.loty = bitcast i192* %b256.hity to i64*
; CHECK-NEXT: store i64 %load256.hi.lo, i64* %b256.hity.loty
; CHECK-NEXT: %b256.hity.hi.gep = getelementptr i64, i64* %b256.hity.loty, i32 1
; CHECK-NEXT: %b256.hity.hity = bitcast i64* %b256.hity.hi.gep to i128*
; CHECK-NEXT: %b256.hity.hity.loty = bitcast i128* %b256.hity.hity to i64*
; CHECK-NEXT: store i64 %load256.hi.hi.lo, i64* %b256.hity.hity.loty
; CHECK-NEXT: %b256.hity.hity.hi.gep = getelementptr i64, i64* %b256.hity.hity.loty, i32 1
; CHECK-NEXT: store i64 %load256.hi.hi.hi, i64* %b256.hity.hity.hi.gep
  store i256 %load256, i256* %b256
  ret void
}

; CHECK-LABEL: @storealign
define void @storealign(i32* %a, i32* %b) {
  %a96 = bitcast i32* %a to i96*
  %b96 = bitcast i32* %b to i96*
  %load96 = load i96, i96* %a96

; CHECK: store i64 %load96.lo{{.*}}, align 16
; CHECK: store i32 %load96.hi{{.*}}, align 8
  store i96 %load96, i96* %b96, align 16

; CHECK: store i64 %load96.lo{{.*}}, align 8
; CHECK: store i32 %load96.hi{{.*}}, align 8
  store i96 %load96, i96* %b96

; CHECK: store i64 %load96.lo{{.*}}, align 4
; CHECK: store i32 %load96.hi{{.*}}, align 4
  store i96 %load96, i96* %b96, align 4

  %a256 = bitcast i32* %a to i256*
  %b256 = bitcast i32* %b to i256*
  %load256 = load i256, i256* %a256
; CHECK: store i64 %load256.lo{{.*}}, align 16
; CHECK: store i64 %load256.hi.lo{{.*}}, align 8
; CHECK: store i64 %load256.hi.hi.lo{{.*}}, align 8
; CHECK: store i64 %load256.hi.hi.hi{{.*}}, align 8
  store i256 %load256, i256* %b256, align 16
  ret void
}


; Check that forward references are handled.
; CHECK-LABEL: @fwdref
define void @fwdref(i32* %a, i32* %b) {
entry:
  br label %block1
block2:
  %b96 = bitcast i32* %b to i96*
; CHECK: store i64 %load96.lo
; CHECK: store i32 %load96.hi
  store i96 %load96, i96* %b96
  ret void
block1:
  %a96 = bitcast i32* %a to i96*
; CHECK: load i64, i64* %a96.loty
; CHECK: load i32, i32* %a96.hity
  %load96 = load i96, i96* %a96
  br label %block2
}

; The subsequent tests use loads and stores to produce and consume the expanded
; values from the opcodes under test.
; CHECK-LABEL: @zext
define void @zext(i32 %a, i64 %b, i8* %p) {
  %p96 = bitcast i8* %p to i96*
  %a96 = zext i32 %a to i96
; CHECK: %a96.lo = zext i32 %a to i64
  store i96 %a96, i96* %p96
; CHECK: store i64 %a96.lo, i64* %p96.loty
; CHECK: store i32 0, i32* %p96.hity

  %b96 = zext i64 %b to i96
; CHECK: store i64 %b, i64* %p96.loty
; CHECK: store i32 0, i32* %p96.hity
  store i96 %b96, i96* %p96

  %p128 = bitcast i8* %p to i128*
  %c96 = load i96, i96* %p96
; CHECK: %a128.hi = zext i32 %c96.hi to i64
  %a128 = zext i96 %c96 to i128
; CHECK: store i64 %c96.lo, i64* %p128.loty
; CHECK: store i64 %a128.hi, i64* %p128.hi.gep
  store i128 %a128, i128* %p128

  %p256 = bitcast i8* %p to i256*

; CHECK: %b256.lo = zext i32 %a to i64
  %b256 = zext i32 %a to i256
; CHECK: store i64 %b256.lo, i64* %p256.loty
; CHECK: store i64 0, i64* %p256.hity.loty
; CHECK: store i64 0, i64* %p256.hity.hity.loty
; CHECK: store i64 0, i64* %p256.hity.hity.hi.gep
  store i256 %b256, i256* %p256

; CHECK: %c256.hi.lo = zext i32 %c96.hi to i64
  %c256 = zext i96 %c96 to i256
; CHECK: store i64 %c96.lo, i64* %p256.loty
; CHECK: store i64 %c256.hi.lo, i64* %p256.hity9.loty
; CHECK: store i64 0, i64* %p256.hity9.hity.loty
; CHECK: store i64 0, i64* %p256.hity9.hity.hi.gep
  store i256 %c256, i256* %p256
   ret void
}


; CHECK-LABEL: @bitwise
define void @bitwise(i32* %a) {
  %a96p = bitcast i32* %a to i96*
  %a96 = load i96, i96* %a96p
  %b96 = load i96, i96* %a96p

; CHECK: %c96.lo = and i64 %a96.lo, %b96.lo
; CHECK: %c96.hi = and i32 %a96.hi, %b96.hi
  %c96 = and i96 %a96, %b96
; CHECK: %d96.lo = or i64 %a96.lo, %c96.lo
; CHECK: %d96.hi = or i32 %a96.hi, %c96.hi
  %d96 = or i96 %a96, %c96

; CHECK: %x96.lo = xor i64 %a96.lo, %c96.lo
; CHECK: %x96.hi = xor i32 %a96.hi, %c96.hi
  %x96 = xor i96 %a96, %c96
  ret void
}

; CHECK-LABEL: @truncs
define void @truncs(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %a96 = load i96, i96* %p96

; CHECK: %t32 = trunc i64 %a96.lo to i32
  %t32 = trunc i96 %a96 to i32

  %b96 = load i96, i96* %p96
; Check that t64 refers directly to the low loaded value from %p96
; CHECK: %t64 = load i64, i64* %p96.loty
  %t64 = trunc i96 %b96 to i64

  %c96 = load i96, i96* %p96
; Use the and to get a use of %t90.lo and check that it refers directly to
; %c96.lo
; CHECK: %t90.hi = trunc i32 %c96.hi to i26
; CHECK: %a90.lo = and i64 %c96.lo, %c96.lo
  %t90 = trunc i96 %c96 to i90
  %t90_2 = trunc i96 %c96 to i90
  %a90 = and i90 %t90, %t90_2
  ret void
}

; CHECK-LABEL: @shls
define void @shls(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %a96 = load i96, i96* %p96
  %p128 = bitcast i32* %p to i128*
  %a128 = load i128, i128* %p128
  %p192 = bitcast i32* %p to i192*
  %a192 = load i192, i192* %p192

; CHECK: %b96.lo = shl i64 %a96.lo, 5
; CHECK-NEXT: %b96.lo.shr = lshr i64 %a96.lo, 59
; CHECK-NEXT: %b96.lo.ext = trunc i64 %b96.lo.shr to i32
; CHECK-NEXT: %b96.hi.shl = shl i32 %a96.hi, 5
; CHECK-NEXT: %b96.or = or i32 %b96.lo.ext, %b96.hi.shl
  %b96 = shl i96 %a96, 5

; CHECK: %d96.lo = shl i64 %a96.lo, 35
; CHECK-NEXT: %d96.lo.shr = lshr i64 %a96.lo, 29
; CHECK-NEXT: %d96.lo.ext = trunc i64 %d96.lo.shr to i32
; CHECK: store i64 %d96.lo, i64* %p96.loty1
; CHECK: store i32 %d96.lo.ext, i32* %p96.hity
  %d96 = shl i96 %a96, 35
  store i96 %d96, i96* %p96

; CHECK: %b128.lo = shl i64 %a128.lo, 35
; CHECK-NEXT: %b128.lo.shr = lshr i64 %a128.lo, 29
; CHECK-NEXT: %b128.hi.shl = shl i64 %a128.hi, 35
; CHECK-NEXT: %b128.or = or i64 %b128.lo.shr, %b128.hi.shl
  %b128 = shl i128 %a128, 35

; CHECK: %c96.lo.ext = trunc i64 %a96.lo to i32
; CHECK-NEXT: %c96.lo.shl = shl i32 %c96.lo.ext, 8
; CHECK: store i64 0, i64* %p96.loty
  %c96 = shl i96 %a96, 72
  store i96 %c96, i96* %p96

; CHECK: %c128.lo.shl = shl i64 %a128.lo, 36
; CHECK: store i64 0, i64* %p128.loty
  %c128 = shl i128 %a128, 100
  store i128 %c128, i128* %p128

; %b192.lo = shl i64 %a192.lo, 35
; %b192.lo.shr = lshr i64 %a192.lo, 29
; %b192.hi.shl.lo = shl i64 %a192.hi.lo, 35
; %b192.hi.shl.lo.shr = lshr i64 %a192.hi.lo, 29
; %b192.hi.shl.hi.shl = shl i64 %a192.hi.hi, 35
; %b192.hi.shl.or = or i64 %b192.hi.shl.lo.shr, %b192.hi.shl.hi.shl
; %b192.or.lo = or i64 %b192.lo.shr, %b192.hi.shl.lo
; %b192.or.hi = or i64 0, %b192.hi.shl.or
  %b192 = shl i192 %a192, 35
  store i192 %b192, i192* %p192

; %c192.lo.shl.lo = shl i64 %a192.lo, 36
; %c192.lo.shl.lo.shr = lshr i64 %a192.lo, 28
; %c192.hi.shl.lo.shl = shl i64 %a192.hi.lo, 36
; %c192.or.lo = or i64 %c192.lo.shl.lo, 0
; %c192.or.hi = or i64 %c192.lo.shl.lo.shr, %c192.hi.shl.lo.shl
  %c192 = shl i192 %a192, 100
  store i192 %c192, i192* %p192

  ret void
}

; CHECK-LABEL: @lshrs
define void @lshrs(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %a96 = load i96, i96* %p96
  %p128 = bitcast i32* %p to i128*
  %a128 = load i128, i128* %p128
  %p192 = bitcast i32* %p to i192*
  %a192 = load i192, i192* %p192

; CHECK:      %b96.hi.shr = lshr i32 %a96.hi, 3
; CHECK-NEXT: %b96.lo.ext = zext i32 %b96.hi.shr to i64
; CHECK:      store i32 0, i32* %p96.hity
  %b96 = lshr i96 %a96, 67
  store i96 %b96, i96* %p96

; CHECK:      %c96.hi.ext = zext i32 %a96.hi to i64
; CHECK-NEXT: %c96.hi.shl = shl i64 %c96.hi.ext, 19
; CHECK-NEXT: %c96.lo.shr = lshr i64 %a96.lo, 45
; CHECK-NEXT: %c96.lo = or i64 %c96.hi.shl, %c96.lo.shr
; CHECK:      store i32 0, i32* %p96.hity
  %c96 = lshr i96 %a96, 45
  store i96 %c96, i96* %p96

; CHECK: %b128.hi.shr = lshr i64 %a128.hi, 3
; CHECK: store i64 0, i64* %p128.hi.gep
  %b128 = lshr i128 %a128, 67
  store i128 %b128, i128* %p128

; CHECK:      %d96.hi.ext = zext i32 %a96.hi to i64
; CHECK-NEXT: %d96.hi.shl = shl i64 %d96.hi.ext, 47
; CHECK-NEXT: %d96.lo.shr = lshr i64 %a96.lo, 17
; CHECK-NEXT: %d96.lo = or i64 %d96.hi.shl, %d96.lo.shr
; CHECK-NEXT: %d96.hi = lshr i32 %a96.hi, 17
  %d96 = lshr i96 %a96, 17
  store i96 %d96, i96* %p96

; CHECK:      %c128.hi.shl = shl i64 %a128.hi, 21
; CHECK-NEXT: %c128.lo.shr = lshr i64 %a128.lo, 43
; CHECK-NEXT: %c128.lo = or i64 %c128.hi.shl, %c128.lo.shr
; CHECK-NEXT: %c128.hi = lshr i64 %a128.hi, 43
  %c128 = lshr i128 %a128, 43
  store i128 %c128, i128* %p128

  %b192 = lshr i192 %a192, 100
  store i192 %b192, i192* %p192

  ret void
}

; Make sure that the following doesn't assert out: it generates intermediate
; `trunc` instructions which get progressively smaller and smaller as the
; instructions are cut down. The final bitcode doesn't contain a `trunc`
; instruction.
;
; CHECK-LABEL: @lshr_big
define void @lshr_big(i32* %a) {
  %p536 = bitcast i32* %a to i536*
  %loaded = load i536, i536* %p536, align 4
  %shifted = lshr i536 %loaded, 161
  store i536 %shifted, i536* %p536
  ret void
}

; CHECK-LABEL: @ashrs
define void @ashrs(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %a96 = load i96, i96* %p96
  %p128 = bitcast i32* %p to i128*
  %a128 = load i128, i128* %p128

; CHECK:      %b96.hi.shr = ashr i32 %a96.hi, 3
; CHECK-NEXT: %b96.lo.ext = sext i32 %b96.hi.shr to i64
; CHECK-NEXT: %b96.hi = ashr i32 %a96.hi, 31
  %b96 = ashr i96 %a96, 67
  store i96 %b96, i96* %p96

; CHECK:      %c96.hi.ext = sext i32 %a96.hi to i64
; CHECK-NEXT: %c96.hi.shl = shl i64 %c96.hi.ext, 19
; CHECK-NEXT: %c96.lo.shr = lshr i64 %a96.lo, 45
; CHECK-NEXT: %c96.lo = or i64 %c96.hi.shl, %c96.lo.shr
; CHECK-NEXT: %c96.hi = ashr i32 %a96.hi, 31
  %c96 = ashr i96 %a96, 45
  store i96 %c96, i96* %p96

; CHECK:      %b128.hi.shr = ashr i64 %a128.hi, 3
; CHECK-NEXT: %b128.hi = ashr i64 %a128.hi, 63
; CHECK:      store i64 %b128.hi, i64* %p128.hi.gep
  %b128 = ashr i128 %a128, 67
  store i128 %b128, i128* %p128

; CHECK:      %d96.hi.ext = sext i32 %a96.hi to i64
; CHECK-NEXT: %d96.hi.shl = shl i64 %d96.hi.ext, 47
; CHECK-NEXT: %d96.lo.shr = lshr i64 %a96.lo, 17
; CHECK-NEXT: %d96.lo = or i64 %d96.hi.shl, %d96.lo.shr
; CHECK-NEXT: %d96.hi = ashr i32 %a96.hi, 17
  %d96 = ashr i96 %a96, 17
  store i96 %d96, i96* %p96

; CHECK:      %c128.hi.shl = shl i64 %a128.hi, 21
; CHECK-NEXT: %c128.lo.shr = lshr i64 %a128.lo, 43
; CHECK-NEXT: %c128.lo = or i64 %c128.hi.shl, %c128.lo.shr
; CHECK-NEXT: %c128.hi = ashr i64 %a128.hi, 43
  %c128 = ashr i128 %a128, 43
  store i128 %c128, i128* %p128

  ret void
}

; CHECK-LABEL: @adds
define void @adds(i32 *%dest, i32* %lhs, i32* %rhs) {
  %d = bitcast i32* %dest to i96*
  %lp = bitcast i32* %lhs to i96*
  %lv = load i96, i96* %lp
  %rp = bitcast i32* %rhs to i96*
  %rv = load i96, i96* %rp

; CHECK: %result.lo = add i64 %lv.lo, %rv.lo
; CHECK-NEXT: %result.cmp = icmp ult i64 %lv.lo, %rv.lo
; CHECK-NEXT: %result.limit = select i1 %result.cmp, i64 %rv.lo, i64 %lv.lo
; CHECK-NEXT: %result.overflowed = icmp ult i64 %result.lo, %result.limit
; CHECK-NEXT: %result.carry = zext i1 %result.overflowed to i32
; CHECK-NEXT: %result.hi = add i32 %lv.hi, %rv.hi
; CHECK-NEXT: %result.carried = add i32 %result.hi, %result.carry
  %result = add i96 %lv, %rv
  store i96 %result, i96* %d
  ret void
}

; CHECK-LABEL: @subs
define void @subs(i32 *%dest, i32* %lhs, i32* %rhs) {
  %d = bitcast i32* %dest to i96*
  %lp = bitcast i32* %lhs to i96*
  %lv = load i96, i96* %lp
  %rp = bitcast i32* %rhs to i96*
  %rv = load i96, i96* %rp

; CHECK: %result.borrow = icmp ult i64 %lv.lo, %rv.lo
; CHECK-NEXT: %result.borrowing = sext i1 %result.borrow to i32
; CHECK-NEXT: %result.lo = sub i64 %lv.lo, %rv.lo
; CHECK-NEXT: %result.hi = sub i32 %lv.hi, %rv.hi
; CHECK-NEXT: %result.borrowed = add i32 %result.hi, %result.borrowing
  %result = sub i96 %lv, %rv
  store i96 %result, i96* %d
  ret void
}

; CHECK-LABEL: @icmp_equality
define void @icmp_equality(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %a96 = load i96, i96* %p96
  %b96 = load i96, i96* %p96

; CHECK: %eq.lo = icmp eq i64 %a96.lo, %b96.lo
; CHECK-NEXT: %eq.hi = icmp eq i32 %a96.hi, %b96.hi
; CHECK-NEXT: %eq = and i1 %eq.lo, %eq.hi
  %eq = icmp eq i96 %a96, %b96

; CHECK: %ne.lo = icmp ne i64 %a96.lo, %b96.lo
; CHECK-NEXT: %ne.hi = icmp ne i32 %a96.hi, %b96.hi
; CHECK-NEXT: %ne = and i1 %ne.lo, %ne.hi
  %ne = icmp ne i96 %a96, %b96
  ret void
}

; CHECK-LABEL: @icmp_uge
define void @icmp_uge(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %lv = load i96, i96* %p96
  %rv = load i96, i96* %p96
; Do an add.
; CHECK: %uge.lo = add i64 %lv.lo, %rv.lo
; CHECK-NEXT: %uge.cmp = icmp ult i64 %lv.lo, %rv.lo
; CHECK-NEXT: %uge.limit = select i1 %uge.cmp, i64 %rv.lo, i64 %lv.lo
; CHECK-NEXT: %uge.overflowed = icmp ult i64 %uge.lo, %uge.limit
; CHECK-NEXT: %uge.carry = zext i1 %uge.overflowed to i32
; CHECK-NEXT: %uge.hi = add i32 %lv.hi, %rv.hi
; CHECK-NEXT: %uge.carried = add i32 %uge.hi, %uge.carry
; Do the hi carry.
; CHECK-NEXT: %uge.cmp4 = icmp ult i32 %lv.hi, %rv.hi
; CHECK-NEXT: %uge.limit5 = select i1 %uge.cmp4, i32 %rv.hi, i32 %lv.hi
; CHECK-NEXT: %uge = icmp ult i32 %uge.carried, %uge.limit5
  %uge = icmp uge i96 %lv, %rv
  ret void
}

; CHECK-LABEL: @icmp_ule
define void @icmp_ule(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %lv = load i96, i96* %p96
  %rv = load i96, i96* %p96
; Do an add.
; CHECK: %ule.lo = add i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ule.cmp = icmp ult i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ule.limit = select i1 %ule.cmp, i64 %rv.lo, i64 %lv.lo
; CHECK-NEXT: %ule.overflowed = icmp ult i64 %ule.lo, %ule.limit
; CHECK-NEXT: %ule.carry = zext i1 %ule.overflowed to i32
; CHECK-NEXT: %ule.hi = add i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ule.carried = add i32 %ule.hi, %ule.carry
; Do the hi carry.
; CHECK-NEXT: %ule.cmp4 = icmp ult i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ule.limit5 = select i1 %ule.cmp4, i32 %rv.hi, i32 %lv.hi
; CHECK-NEXT: %ule.overflowed6 = icmp ult i32 %ule.carried, %ule.limit5
; Invert the carry result.
; CHECK-NEXT: %ule = xor i1 %ule.overflowed6, true
  %ule = icmp ule i96 %lv, %rv
  ret void
}

; CHECK-LABEL: @icmp_ugt
define void @icmp_ugt(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %lv = load i96, i96* %p96
  %rv = load i96, i96* %p96
; Do an add.
; CHECK: %ugt.lo = add i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ugt.cmp = icmp ult i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ugt.limit = select i1 %ugt.cmp, i64 %rv.lo, i64 %lv.lo
; CHECK-NEXT: %ugt.overflowed = icmp ult i64 %ugt.lo, %ugt.limit
; CHECK-NEXT: %ugt.carry = zext i1 %ugt.overflowed to i32
; CHECK-NEXT: %ugt.hi = add i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ugt.carried = add i32 %ugt.hi, %ugt.carry
; Do the hi carry.
; CHECK-NEXT: %ugt.cmp4 = icmp ult i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ugt.limit5 = select i1 %ugt.cmp4, i32 %rv.hi, i32 %lv.hi
; CHECK-NEXT: %ugt.overflowed6 = icmp ult i32 %ugt.carried, %ugt.limit5
; Equality comparison.
; CHECK-NEXT: %ugt.lo7 = icmp eq i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ugt.hi8 = icmp eq i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ugt.eq = and i1 %ugt.lo7, %ugt.hi8
; Merge the hi carry and equality comparison results.
; CHECK-NEXT: %ugt = and i1 %ugt.overflowed6, %ugt.eq
  %ugt = icmp ugt i96 %lv, %rv
  ret void
}

; CHECK-LABEL: @icmp_ult
define void @icmp_ult(i32* %p) {
  %p96 = bitcast i32* %p to i96*
  %lv = load i96, i96* %p96
  %rv = load i96, i96* %p96
; Do an add.
; CHECK: %ult.lo = add i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ult.cmp = icmp ult i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ult.limit = select i1 %ult.cmp, i64 %rv.lo, i64 %lv.lo
; CHECK-NEXT: %ult.overflowed = icmp ult i64 %ult.lo, %ult.limit
; CHECK-NEXT: %ult.carry = zext i1 %ult.overflowed to i32
; CHECK-NEXT: %ult.hi = add i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ult.carried = add i32 %ult.hi, %ult.carry
; Do the hi carry.
; CHECK-NEXT: %ult.cmp4 = icmp ult i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ult.limit5 = select i1 %ult.cmp4, i32 %rv.hi, i32 %lv.hi
; CHECK-NEXT: %ult.overflowed6 = icmp ult i32 %ult.carried, %ult.limit5
; Invert the carry result.
; CHECK-NEXT: %ult7 = xor i1 %ult.overflowed6, true
; Equality comparison.
; CHECK-NEXT: %ult.lo8 = icmp eq i64 %lv.lo, %rv.lo
; CHECK-NEXT: %ult.hi9 = icmp eq i32 %lv.hi, %rv.hi
; CHECK-NEXT: %ult.eq = and i1 %ult.lo8, %ult.hi9
; Merge the hi carry and equality comparison results.
; CHECK-NEXT: %ult = and i1 %ult7, %ult.eq
  %ult = icmp ult i96 %lv, %rv
  ret void
}

; CHECK-LABEL: @selects
define void @selects(i1 %c, i32* %pl, i32* %pr) {
  %pl96 = bitcast i32* %pl to i96*
  %pr96 = bitcast i32* %pr to i96*
  %l = load i96, i96* %pl96
  %r = load i96, i96* %pr96

; CHECK: %result.lo = select i1 %c, i64 %l.lo, i64 %r.lo
; CHECK-NEXT: %result.hi = select i1 %c, i32 %l.hi, i32 %r.hi
  %result = select i1 %c, i96 %l, i96 %r
  ret void
}

; CHECK-LABEL: @phis1
define void @phis1() {
entry:
  br label %label1
label1:
  br i1 undef, label %label2, label %end
label2:
  br label %end
end:
; CHECK: %foo.lo = phi i64 [ undef, %label1 ], [ undef, %label2 ]
; CHECK-NEXT: %foo.hi = phi i8 [ undef, %label1 ], [ undef, %label2 ]
; CHECK-NEXT: %bar.lo = and i64 %foo.lo, 137438953472
; CHECK-NEXT: %bar.hi = and i8 %foo.hi, 0
  %foo = phi i72 [ undef, %label1 ], [ undef, %label2 ]
  %bar = and i72 %foo, 137438953472
  br i1 undef, label %label1, label %label2
}

; CHECK-LABEL: @phis2
define void @phis2() {
entry:
  br label %label1
label1:
; CHECK: %foo.lo = phi i64 [ %bar.lo, %label2 ], [ undef, %entry ]
; CHECK-NEXT:  %foo.hi = phi i8 [ %bar.hi, %label2 ], [ undef, %entry ]
  %foo = phi i72 [ %bar, %label2 ], [ undef, %entry ]
  br i1 undef, label %label2, label %end
label2:
; CHECK: %bar.lo = load i64, i64* undef, align 4
; CHECK-NEXT: %bar.hi = load i8, i8* undef, align 4
  %bar = load i72, i72* undef, align 4
  br label %label1
end:
  ret void
}
