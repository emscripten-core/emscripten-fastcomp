; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -O0 -o - \
; RUN:   | FileCheck %s

; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -O2 -o - \
; RUN:   | FileCheck %s

; Check that we don't try to fold a negative displacement into a memory
; reference
define i16 @negativedisp(i32 %b) {
; CHECK: negativedisp
  %a = alloca [1 x i16], align 2
  %add = add nsw i32 1073741824, %b
  %arrayidx = getelementptr inbounds [1 x i16]* %a, i32 0, i32 %add
; CHECK-NOT: nacl:-2147483648(
  %c = load i16* %arrayidx, align 2
  ret i16 %c
}

@main.m2 = internal constant [1 x [1 x i32]] [[1 x i32] [i32 -60417067]], align 4
define i1 @largeconst() nounwind {
; CHECK: largeconst
entry:
  %retval = alloca i32, align 4
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %madat = alloca i32*, align 4
  store i32 0, i32* %retval
  store i32 -270770481, i32* %i, align 4
  store i32 -1912319477, i32* %j, align 4
  %0 = load i32* %j, align 4
  %mul = mul nsw i32 %0, 233468377
  %add = add nsw i32 %mul, 689019309
  %1 = load i32* %i, align 4
  %mul1 = mul nsw i32 %1, 947877507
  %add2 = add nsw i32 %mul1, 1574375955
  %arrayidx = getelementptr inbounds [1 x i32]* getelementptr inbounds ([1 x [1 x i32]]* @main.m2, i32 0, i32 0), i32 %add2
  %2 = bitcast [1 x i32]* %arrayidx to i32*
  %arrayidx3 = getelementptr inbounds i32* %2, i32 %add
  store i32* %arrayidx3, i32** %madat, align 4
; Ensure the large constant doesn't get folded into the load
; CHECK: nacl:(%r15
  %3 = load i32** %madat, align 4
  %4 = load i32* %3, align 4
  %conv = zext i32 %4 to i64
  %5 = load i32* %j, align 4
  %mul4 = mul nsw i32 %5, 233468377
  %add5 = add nsw i32 %mul4, 689019309
  %6 = load i32* %i, align 4
  %mul6 = mul nsw i32 %6, 947877507
  %add7 = add nsw i32 %mul6, 1574375955
  %arrayidx8 = getelementptr inbounds [1 x i32]* getelementptr inbounds ([1 x [1 x i32]]* @main.m2, i32 0, i32 0), i32 %add7
  %7 = bitcast [1 x i32]* %arrayidx8 to i32*
  %arrayidx9 = getelementptr inbounds i32* %7, i32 %add5
; Ensure the large constant doesn't get folded into the load
; CHECK: nacl:(%r15
  %8 = load i32* %arrayidx9, align 4
  %conv10 = zext i32 %8 to i64
  %mul11 = mul nsw i64 3795428823, %conv10
  %9 = load i32* %j, align 4
  %mul12 = mul nsw i32 %9, 233468377
  %add13 = add nsw i32 %mul12, 689019309
  %conv14 = sext i32 %add13 to i64
  %rem = srem i64 %conv14, 4294967295
  %xor = xor i64 2597389499, %rem
  %mul15 = mul nsw i64 %xor, 3795428823
  %sub = sub nsw i64 %mul11, %mul15
  %add16 = add nsw i64 %sub, 3829710203
  %mul17 = mul nsw i64 %add16, 2824337475
  %add18 = add nsw i64 %mul17, 2376483023
  %cmp = icmp eq i64 %conv, %add18
  ret i1 %cmp
}


@main.array = private unnamed_addr constant [1 x i64] [i64 1438933078946427748], align 8

define i1 @largeconst_frameindex() nounwind {
; CHECK: largeconst_frameindex
entry:
  %retval = alloca i32, align 4
  %r_Ng = alloca i64, align 8
  %i = alloca i32, align 4
  %adat = alloca i64*, align 4
  %array = alloca [1 x i64], align 8
  store i32 0, i32* %retval
  store i32 -270770481, i32* %i, align 4
  %0 = bitcast [1 x i64]* %array to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %0, i8* bitcast ([1 x i64]* @main.array to i8*), i32 8, i32 8, i1 false)
  store i32 -270770481, i32* %i, align 4
  %1 = load i32* %i, align 4
  %mul = mul i32 %1, 947877507
  %add = add i32 %mul, 1574375955
  %2 = bitcast [1 x i64]* %array to i64*
  %arrayidx = getelementptr inbounds i64* %2, i32 %add
; Ensure the large constant didn't get folded into the load
; CHECK: nacl:(%r15
  %3 = load i64* %arrayidx, align 8
  %add1 = add i64 %3, -5707596139582126917
  %4 = load i32* %i, align 4
  %mul2 = mul i32 %4, 947877507
  %add3 = add i32 %mul2, 1574375955
  %5 = bitcast [1 x i64]* %array to i64*
  %arrayidx4 = getelementptr inbounds i64* %5, i32 %add3
  store i64 %add1, i64* %arrayidx4, align 8
  %6 = load i32* %i, align 4
  %mul5 = mul nsw i32 %6, 947877507
  %add6 = add nsw i32 %mul5, 1574375955
  %arrayidx7 = getelementptr inbounds [1 x i64]* %array, i32 0, i32 %add6
; CHECK: nacl:(%r15
  %7 = load i64* %arrayidx7, align 8
  %add8 = add i64 %7, -5707596139582126917
  %8 = load i32* %i, align 4
  %mul9 = mul nsw i32 %8, 947877507
  %add10 = add nsw i32 %mul9, 1574375955
  %arrayidx11 = getelementptr inbounds [1 x i64]* %array, i32 0, i32 %add10
  store i64 %add8, i64* %arrayidx11, align 8
  %9 = load i32* %i, align 4
  %mul12 = mul nsw i32 %9, 947877507
  %add13 = add nsw i32 %mul12, 1574375955
  %10 = bitcast [1 x i64]* %array to i64*
  %arrayidx14 = getelementptr inbounds i64* %10, i32 %add13
  store i64* %arrayidx14, i64** %adat, align 4
  %11 = load i64** %adat, align 4
  %12 = load i64* %11, align 8
  %mul15 = mul i64 %12, -1731288434922394955
  %add16 = add i64 %mul15, -7745351015538694962
  store i64 %add16, i64* %r_Ng, align 8
  ret i1 0
}

declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture, i32, i32, i1) nounwind
