; RUN: opt < %s -nacl-promote-ints -S | FileCheck %s

declare void @consume_i16(i16 %a)

; CHECK: @sext_to_illegal
; CHECK-NEXT: %a40 = sext i32 %a to i64
; (0xFFFFFFFFFF)
define void @sext_to_illegal(i32 %a) {
  %a40 = sext i32 %a to i40
  ret void
}

; CHECK; @sext_from_illegal
define void @sext_from_illegal(i8 %a) {
; CHECK: call void @consume_i16(i16 -2)
  %c12 = sext i12 -2 to i16
  call void @consume_i16(i16 %c12)
; CHECK: %a12 = sext i8 %a to i16
  %a12 = sext i8 %a to i12
; CHECK: %a12.getsign = shl i16 %a12, 4
; CHECK-NEXT: %a16 = ashr i16 %a12.getsign, 4
  %a16 = sext i12 %a12 to i16
; CHECK: %a12.getsign1 = shl i16 %a12, 4
; CHECK-NEXT: %a14 = ashr i16 %a12.getsign1, 4
; (0x3FFF)
  %a14 = sext i12 %a12 to i14
; CHECK-NEXT: %a12.getsign2 = shl i16 %a12, 4
; CHECK-NEXT: %a12.signed = ashr i16 %a12.getsign2, 4
; CHECK-NEXT: %a24 = sext i16 %a12.signed to i32
; (0xFFFFFF)
  %a24 = sext i12 %a12 to i24

  %a37 = zext i8 %a to i37
; CHECK: %a37.getsign = shl i64 %a37, 27
; CHECK-NEXT: %a64 = ashr i64 %a37.getsign, 27
  %a64 = sext i37 %a37 to i64
  ret void
}

; CHECK: @zext_to_illegal
define void @zext_to_illegal(i32 %a) {
; CHECK: zext i32 %a to i64
; CHECK-NOT: and
  %a40 = zext i32 %a to i40
  ret void
}

; CHECK: @zext_from_illegal
define void @zext_from_illegal(i8 %a) {
; get some illegal values to start with
  %a24 = zext i8 %a to i24
  %a40 = zext i8 %a to i40
  %a18 = zext i8 %a to i18

; CHECK: %a32 = and i32 %a24, 16777215
; (0xFFFFFF)
  %a32 = zext i24 %a24 to i32

; CHECK: %b24 = and i32 %a18, 262143
; (0x3FFFF)
  %b24 = zext i18 %a18 to i24

; CHECK: %a24.clear = and i32 %a24, 16777215
; CHECK: %b40 = zext i32 %a24.clear to i64
  %b40 = zext i24 %a24 to i40

; CHECK: call void @consume_i16(i16 4094)
  %c16 = zext i12 -2 to i16
  call void @consume_i16(i16 %c16)
; CHECK: call void @consume_i16(i16 4094)
  %c14 = zext i12 -2 to i14
  %c16.2 = zext i14 %c14 to i16
  call void @consume_i16(i16 %c16.2)
  ret void
}

; CHECK: @trunc_from_illegal
define void @trunc_from_illegal(i8 %a) {
  %a24 = zext i8 %a to i24
; CHECK: %a16 = trunc i32 %a24 to i16
  %a16 = trunc i24 %a24 to i16
  ret void
}

; CHECK: @trunc_to_illegal
define void @trunc_to_illegal(i8 %a8) {
  %a = zext i8 %a8 to i32
; CHECK-NOT: trunc i32 %a
; CHECK-NOT: and
  %a24 = trunc i32 %a to i24

; CHECK: %a12 = trunc i32 %a24 to i16
; CHECK-NOT: and
  %a12 = trunc i24 %a24 to i12
  ret void
}

; CHECK: @icmpsigned
define void @icmpsigned(i32 %a) {
  %shl = trunc i32 %a to i24
; CHECK: %shl.getsign = shl i32 %shl, 8
; CHECK-NEXT: %shl.signed = ashr i32 %shl.getsign, 8
; CHECK-NEXT: %cmp = icmp slt i32 %shl.signed, -2
  %cmp = icmp slt i24 %shl, -2
  ret void
}

; Bitcasts are left unchanged.
%struct.ints = type { i32, i32 }
; CHECK: @bc1
; CHECK-NEXT: %bc1 = bitcast i32* %a to i40*
; CHECK-NEXT: %bc2 = bitcast i40* %bc1 to i32*
; CHECK-NEXT: %bc3 = bitcast %struct.ints* null to i40*
; CHECK-NEXT: %bc4 = bitcast i40* %bc1 to %struct.ints*
define i32* @bc1(i32* %a) {
  %bc1 = bitcast i32* %a to i40*
  %bc2 = bitcast i40* %bc1 to i32*
  %bc3 = bitcast %struct.ints* null to i40*
  %bc4 = bitcast i40* %bc1 to %struct.ints*
  ret i32* %bc2
}

; CHECK: zext i32 %a to i64
; CHECK: and i64 %a40, 255
define void @and1(i32 %a) {
  %a40 = zext i32 %a to i40
  %and = and i40 %a40, 255
  ret void
}

; CHECK: @andi3
define void @andi3(i8 %a) {
  %a3 = trunc i8 %a to i3
; CHECK: and i8 %a3, 2
  %and = and i3 %a3, 2
  ret void
}

; CHECK: @ori7
define void @ori7(i8 %a, i8 %b) {
  %a7 = trunc i8 %a to i7
  %b7 = trunc i8 %b to i7
; CHECK: %or = or i8 %a7, %b7
  %or = or i7 %a7, %b7
  ret void
}

; CHECK: @add1
define void @add1(i16 %a) {
; CHECK-NEXT: %a24 = sext i16 %a to i32
  %a24 = sext i16 %a to i24
; CHECK-NEXT: %sum = add i32 %a24, 16777214
  %sum = add i24 %a24, -2
; CHECK-NEXT: %sumnsw = add nsw i32 %a24, 16777214
  %sumnsw = add nsw i24 %a24, -2
; CHECK-NEXT: %sumnuw = add nuw i32 %a24, 16777214
  %sumnuw = add nuw i24 %a24, -2
; CHECK-NEXT: %sumnw = add nuw nsw i32 %a24, 16777214
  %sumnw = add nuw nsw i24 %a24, -2
  ret void
}

; CHECK: @mul1
define void @mul1(i32 %a, i32 %b) {
; CHECK-NEXT: %a33 = sext i32 %a to i64
  %a33 = sext i32 %a to i33
; CHECK-NEXT: %b33 = sext i32 %b to i64
  %b33 = sext i32 %b to i33
; CHECK-NEXT: %product = mul i64 %a33, %b33
  %product = mul i33 %a33, %b33
; CHECK-NEXT: %prodnw = mul nuw nsw i64 %a33, %b33
  %prodnw = mul nuw nsw i33 %a33, %b33
  ret void
}

; CHECK: @shl1
define void @shl1(i16 %a) {
  %a24 = zext i16 %a to i24
; CHECK: %ashl = shl i32 %a24, 5
  %ashl = shl i24 %a24, 5

; CHECK-NEXT: %ashl2 = shl i32 %a24, 1
  %ashl2 = shl i24 %a24, 4278190081 ;0xFF000001

  %b24 = zext i16 %a to i24
; CHECK: %b24.clear = and i32 %b24, 16777215
; CHECK-NEXT: %bshl = shl i32 %a24, %b24.clear
  %bshl = shl i24 %a24, %b24
  ret void
}

; CHECK: @shlnuw
define void @shlnuw(i16 %a) {
  %a12 = trunc i16 %a to i12
; CHECK: %ashl = shl nuw i16 %a12, 5
  %ashl = shl nuw i12 %a12, 5
  ret void
}

; CHECK: @lshr1
define void @lshr1(i16 %a) {
  %a24 = zext i16 %a to i24
; CHECK: %a24.clear = and i32 %a24, 16777215
; CHECK-NEXT: %b = lshr i32 %a24.clear, 20
  %b = lshr i24 %a24, 20
; CHECK-NEXT: %a24.clear1 = and i32 %a24, 16777215
; CHECK-NEXT: %c = lshr i32 %a24.clear1, 5
  %c = lshr i24 %a24, 5

  %b24 = zext i16 %a to i24
  %d = lshr i24 %a24, %b24
; CHECK: %a24.clear2 = and i32 %a24, 16777215
; CHECK-NEXT: %b24.clear = and i32 %b24, 16777215
; CHECK-NEXT: %d = lshr i32 %a24.clear2, %b24.clear
  ret void
}

; CHECK: @ashr1
define void @ashr1(i16 %a) {
  %a24 = sext i16 %a to i24
; CHECK: %a24.getsign = shl i32 %a24, 8
; CHECK-NEXT: %b24 = ashr i32 %a24.getsign, 19
  %b24 = ashr i24 %a24, 11
; CHECK-NEXT: %a24.getsign1 = shl i32 %a24, 8
; CHECK-NEXT: %b24.clear = and i32 %b24, 16777215
; CHECK-NEXT: %a24.shamt = add i32 %b24.clear, 8
; CHECK-NEXT: %c = ashr i32 %a24.getsign1, %a24.shamt
  %c = ashr i24 %a24, %b24
  ret void
}

; CHECK: @phi_icmp
define void @phi_icmp(i32 %a) {
entry:
  br label %loop
loop:
; CHECK: %phi40 = phi i64 [ 1099511627774, %entry ], [ %phi40, %loop ]
  %phi40 = phi i40 [ -2, %entry ],  [ %phi40, %loop ]
; CHECK-NEXT: %phi40.clear = and i64 %phi40, 1099511627775
; CHECK-NEXT: %b = icmp eq i64 %phi40.clear, 1099511627775
  %b = icmp eq i40 %phi40, -1
; CHECK-NEXT: br i1 %b, label %loop, label %end
  br i1 %b, label %loop, label %end
end:
  ret void
}

; CHECK: @icmp_ult
define void @icmp_ult(i32 %a) {
  %a40 = zext i32 %a to i40
; CHECK: %a40.clear = and i64 %a40, 1099511627775
; CHECK-NEXT: %b = icmp ult i64 %a40.clear, 1099511627774
  %b = icmp ult i40 %a40, -2

; CHECK: %a40.clear1 = and i64 %a40, 1099511627775
; CHECK-NEXT: %b40.clear = and i64 %b40, 1099511627775
; CHECK-NEXT: %c = icmp ult i64 %a40.clear1, %b40.clear
  %b40 = zext i32 %a to i40
  %c = icmp ult i40 %a40, %b40
  ret void
}

; CHECK: @select1
define void @select1(i32 %a) {
  %a40 = zext i32 %a to i40
; CHECK: %s40 = select i1 true, i64 %a40, i64 1099511627775
  %s40 = select i1 true, i40 %a40, i40 -1
  ret void
}

; Allocas are left unchanged.
; CHECK: @alloca40
; CHECK: %a = alloca i40, align 8
define void @alloca40() {
  %a = alloca i40, align 8
  %b = bitcast i40* %a to i8*
  %c = load i8* %b
  ret void
}

; CHECK: @load24
; CHECK: %bc.loty = bitcast i24* %bc to i16*
; CHECK-NEXT: %load.lo = load i16* %bc.loty
; CHECK-NEXT: %load.lo.ext = zext i16 %load.lo to i32
; CHECK-NEXT: %bc.hi = getelementptr i16* %bc.loty, i32 1
; CHECK-NEXT: %bc.hity = bitcast i16* %bc.hi to i8*
; CHECK-NEXT: %load.hi = load i8* %bc.hity
; CHECK-NEXT: %load.hi.ext = zext i8 %load.hi to i32
; CHECK-NEXT: %load.hi.ext.sh = shl i32 %load.hi.ext, 16
; CHECK-NEXT: %load = or i32 %load.lo.ext, %load.hi.ext.sh
define void @load24(i8* %a) {
  %bc = bitcast i8* %a to i24*
  %load = load i24* %bc, align 8
  ret void
}

; CHECK: @load48
; CHECK: %bc.loty = bitcast i48* %bc to i32*
; CHECK-NEXT: %load.lo = load i32* %bc.loty
; CHECK-NEXT: %load.lo.ext = zext i32 %load.lo to i64
; CHECK-NEXT: %bc.hi = getelementptr i32* %bc.loty, i32 1
; CHECK-NEXT: %bc.hity = bitcast i32* %bc.hi to i16*
; CHECK-NEXT: %load.hi = load i16* %bc.hity
; CHECK-NEXT: %load.hi.ext = zext i16 %load.hi to i64
; CHECK-NEXT: %load.hi.ext.sh = shl i64 %load.hi.ext, 32
; CHECK-NEXT: %load = or i64 %load.lo.ext, %load.hi.ext.sh
define void @load48(i32* %a) {
  %bc = bitcast i32* %a to i48*
  %load = load i48* %bc, align 8
  ret void
}

; CHECK: @load56
; CHECK:  %bc = bitcast i32* %a to i56*
; CHECK-NEXT:  %bc.loty = bitcast i56* %bc to i32*
; CHECK-NEXT:  %load.lo = load i32* %bc.loty
; CHECK-NEXT:  %load.lo.ext = zext i32 %load.lo to i64
; CHECK-NEXT:  %bc.hi = getelementptr i32* %bc.loty, i32 1
; CHECK-NEXT:  %bc.hity = bitcast i32* %bc.hi to i24*
; CHECK-NEXT:  %bc.hity.loty = bitcast i24* %bc.hity to i16*
; CHECK-NEXT:  %load.hi.lo = load i16* %bc.hity.loty
; CHECK-NEXT:  %load.hi.lo.ext = zext i16 %load.hi.lo to i32
; CHECK-NEXT:  %bc.hity.hi = getelementptr i16* %bc.hity.loty, i32 1
; CHECK-NEXT:  %bc.hity.hity = bitcast i16* %bc.hity.hi to i8*
; CHECK-NEXT:  %load.hi.hi = load i8* %bc.hity.hity
; CHECK-NEXT:  %load.hi.hi.ext = zext i8 %load.hi.hi to i32
; CHECK-NEXT:  %load.hi.hi.ext.sh = shl i32 %load.hi.hi.ext, 16
; CHECK-NEXT:  %load.hi = or i32 %load.hi.lo.ext, %load.hi.hi.ext.sh
; CHECK-NEXT:  %load.hi.ext = zext i32 %load.hi to i64
; CHECK-NEXT:  %load.hi.ext.sh = shl i64 %load.hi.ext, 32
; CHECK-NEXT:  %load = or i64 %load.lo.ext, %load.hi.ext.sh
define void @load56(i32* %a) {
  %bc = bitcast i32* %a to i56*
  %load = load i56* %bc
  ret void
}

; CHECK: @store24
; CHECK: %b24 = zext i8 %b to i32
; CHECK-NEXT: %bc.loty = bitcast i24* %bc to i16*
; CHECK-NEXT: %b24.lo = trunc i32 %b24 to i16
; CHECK-NEXT: store i16 %b24.lo, i16* %bc.loty
; CHECK-NEXT: %b24.hi.sh = lshr i32 %b24, 16
; CHECK-NEXT: %bc.hi = getelementptr i16* %bc.loty, i32 1
; CHECK-NEXT: %b24.hi = trunc i32 %b24.hi.sh to i8
; CHECK-NEXT: %bc.hity = bitcast i16* %bc.hi to i8*
; CHECK-NEXT: store i8 %b24.hi, i8* %bc.hity
define void @store24(i8* %a, i8 %b) {
  %bc = bitcast i8* %a to i24*
  %b24 = zext i8 %b to i24
  store i24 %b24, i24* %bc
  ret void
}

; CHECK: @store56
; CHECK: %b56 = zext i8 %b to i64
; CHECK-NEXT: %bc.loty = bitcast i56* %bc to i32*
; CHECK-NEXT: %b56.lo = trunc i64 %b56 to i32
; CHECK-NEXT: store i32 %b56.lo, i32* %bc.loty
; CHECK-NEXT: %b56.hi.sh = lshr i64 %b56, 32
; CHECK-NEXT: %bc.hi = getelementptr i32* %bc.loty, i32 1
; CHECK-NEXT: %bc.hity = bitcast i32* %bc.hi to i24*
; CHECK-NEXT: %bc.hity.loty = bitcast i24* %bc.hity to i16*
; CHECK-NEXT: %b56.hi.sh.lo = trunc i64 %b56.hi.sh to i16
; CHECK-NEXT: store i16 %b56.hi.sh.lo, i16* %bc.hity.loty
; CHECK-NEXT: %b56.hi.sh.hi.sh = lshr i64 %b56.hi.sh, 16
; CHECK-NEXT: %bc.hity.hi = getelementptr i16* %bc.hity.loty, i32 1
; CHECK-NEXT: %b56.hi.sh.hi = trunc i64 %b56.hi.sh.hi.sh to i8
; CHECK-NEXT: %bc.hity.hity = bitcast i16* %bc.hity.hi to i8*
; CHECK-NEXT: store i8 %b56.hi.sh.hi, i8* %bc.hity.hity
define void @store56(i8* %a, i8 %b) {
  %bc = bitcast i8* %a to i56*
  %b56 = zext i8 %b to i56
  store i56 %b56, i56* %bc
  ret void
}

; CHECK: @undefoperand
; CHECK-NEXT: %a40 = zext i32 %a to i64
; CHECK-NEXT: %au = and i64 %a40, undef
define void @undefoperand(i32 %a) {
  %a40 = zext i32 %a to i40
  %au = and i40 %a40, undef
  ret void
}

; CHECK: @switch
; CHECK-NEXT: %a24 = zext i16 %a to i32
; CHECK-NEXT: %a24.clear = and i32 %a24, 16777215
; CHECK-NEXT: switch i32 %a24.clear, label %end [
; CHECK-NEXT: i32 0, label %if1
; CHECK-NEXT: i32 1, label %if2
define void @switch(i16 %a) {
  %a24 = zext i16 %a to i24
  switch i24 %a24, label %end [
    i24 0, label %if1
    i24 1, label %if2
  ]
if1:
  ret void
if2:
  ret void
end:
  ret void
}


; The getelementptr here should be handled unchanged.
; CHECK: @pointer_to_array
; CHECK: %element_ptr = getelementptr [2 x i40]* %ptr, i32 0, i32 0
define void @pointer_to_array([2 x i40]* %ptr) {
  %element_ptr = getelementptr [2 x i40]* %ptr, i32 0, i32 0
  load i40* %element_ptr
  ret void
}
