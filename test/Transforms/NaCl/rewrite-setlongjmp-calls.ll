; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s
; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s -check-prefix=CLEANED
; Test the RewritePNaClLibraryCalls pass

declare i32 @setjmp(i64*)
declare void @longjmp(i64*, i32)

; No declaration or definition of setjmp() should remain.
; CLEANED-NOT: @setjmp

; Since the address of longjmp is being taken here, a body is generated
; for it, which does a cast and calls an intrinsic

; CHECK: define internal void @longjmp(i64* %env, i32 %val) {
; CHECK: entry:
; CHECK:   %jmp_buf_i8 = bitcast i64* %env to i8*
; CHECK:   call void @llvm.nacl.longjmp(i8* %jmp_buf_i8, i32 %val)
; CHECK:   unreachable
; CHECK: }

define i32 @call_setjmp(i64* %arg) {
; CHECK-NOT: call i32 @setjmp
; CHECK: %jmp_buf_i8 = bitcast i64* %arg to i8*
; CHECK-NEXT: %val = call i32 @llvm.nacl.setjmp(i8* %jmp_buf_i8)
  %val = call i32 @setjmp(i64* %arg)
  ret i32 %val
}

define void @call_longjmp(i64* %arg, i32 %num) {
; CHECK-NOT: call void @longjmp
; CHECK: %jmp_buf_i8 = bitcast i64* %arg to i8*
; CHECK-NEXT: call void @llvm.nacl.longjmp(i8* %jmp_buf_i8, i32 %num)
  call void @longjmp(i64* %arg, i32 %num)
  ret void
}

define i32 @takeaddr_longjmp(i64* %arg, i32 %num) {
  %fp = alloca void (i64*, i32)*, align 8
; CHECK: store void (i64*, i32)* @longjmp, void (i64*, i32)** %fp
  store void (i64*, i32)* @longjmp, void (i64*, i32)** %fp, align 8
  ret i32 7
}

; A more complex example with a number of calls in several BBs
define void @multiple_calls(i64* %arg, i32 %num) {
entryblock:
; CHECK: entryblock
; CHECK: bitcast
; CHECK-NEXT: call void @llvm.nacl.longjmp
  call void @longjmp(i64* %arg, i32 %num)
  br label %block1
block1:
; CHECK: block1
; CHECK: bitcast
; CHECK-NEXT: call void @llvm.nacl.longjmp
  call void @longjmp(i64* %arg, i32 %num)
; CHECK: call i32 @llvm.nacl.setjmp
  %val = call i32 @setjmp(i64* %arg)
  %num2 = add i32 %val, %num
; CHECK: bitcast
; CHECK-NEXT: call void @llvm.nacl.longjmp
  call void @longjmp(i64* %arg, i32 %num2)
  br label %exitblock
exitblock:
  %num3 = add i32 %num, %num
  call void @longjmp(i64* %arg, i32 %num3)
  %num4 = add i32 %num, %num3
; CHECK: bitcast
; CHECK-NEXT: call void @llvm.nacl.longjmp
  call void @longjmp(i64* %arg, i32 %num4)
  ret void
}

; CHECK: declare i32 @llvm.nacl.setjmp(i8*)
; CHECK: declare void @llvm.nacl.longjmp(i8*, i32)

