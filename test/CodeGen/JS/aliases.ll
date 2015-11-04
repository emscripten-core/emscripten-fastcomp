; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

@.str = private unnamed_addr constant [18 x i8] c"hello, world! %d\0A\00", align 1 ; [#uses=1 type=[18 x i8]*]

@othername = internal alias void (i32), void (i32)* @doit
@othername2 = internal alias void (i32), void (i32)* @othername
@othername3 = internal alias void (i32), void (i32)* @othername2
@othername4 = internal alias void (), bitcast (void (i32)* @othername2 to void ()*)

@list = global i32 ptrtoint (void ()* @othername4 to i32)
@list2 = global <{ i32, i32, i32, i32, i32 }> <{ i32 ptrtoint (void (i32)* @doit to i32), i32 ptrtoint (void (i32)* @othername to i32), i32 ptrtoint (void (i32)* @othername2 to i32), i32 ptrtoint (void (i32)* @othername3 to i32), i32 ptrtoint (void ()* @othername4 to i32) }>


@value = global i32 17
@value2 = alias i32, i32* @value
@value3 = alias i32, i32* @value

define internal void @doit(i32 %x) {
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @.str, i32 0, i32 0), i32 %x) ; [#uses=0 type=i32]
  ret void
}

;;; we just check for compilation to succeed here, specifically of @list and @list2
; CHECK: function _main() {
; CHECK: }

define i32 @main() {
entry:
  call void () @othername4()
  %fp = ptrtoint void ()* @othername4 to i32
  %fp1 = add i32 %fp, 0
  %pf = inttoptr i32 %fp1 to void (i32)*
  %x = load i32, i32* @value3
  call void (i32) %pf(i32 %x)
  %x1 = load i32, i32* @value2
  call void (i32) @othername3(i32 %x1)
  %x2 = load i32, i32* @value
  call void (i32) @othername2(i32 %x2)
  store i32 18, i32* @value
  %x3 = load i32, i32* @value
  call void (i32) @othername(i32 %x3)
  store i32 19, i32* @value3
  %x4 = load i32, i32* @value3
  call void (i32) @doit(i32 %x4)
  ret i32 1
}

declare i32 @printf(i8*, ...)

