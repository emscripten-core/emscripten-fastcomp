; RUN: llc < %s | FileCheck %s

; Basic AllocaManager feature test. Eliminate user variable cupcake in favor of
; user variable muffin, and combine all the vararg buffers. And align the stack
; pointer.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

%struct._IO_FILE = type opaque

@stderr = external constant [4 x i8], align 4
@.str = private unnamed_addr constant [26 x i8] c"hello from %s; argc is %d\00", align 1
@.str1 = private unnamed_addr constant [33 x i8] c"message from the program: \22%s\22!\0A\00", align 1
@.str2 = private unnamed_addr constant [38 x i8] c"with argc %d, I, %s, must say goodbye\00", align 1
@.str3 = private unnamed_addr constant [43 x i8] c"another message from the program: \22%s\22...\0A\00", align 1

; CHECK: function _foo($argc,$argv) {
; CHECK-NOT: cupcake
; CHECK: STACKTOP = STACKTOP + 128|0;
; CHECK-NEXT: vararg_buffer0 =
; CHECK-NEXT: $muffin =
; CHECK-NOT: cupcake
; CHECK: }

; Function Attrs: nounwind
define void @foo(i32 %argc, i8** %argv) #0 {
entry:
  %vararg_buffer0 = alloca <{ i8* }>, align 8
  %vararg_lifetime_bitcast10 = bitcast <{ i8* }>* %vararg_buffer0 to i8*
  %vararg_buffer5 = alloca <{ i32, i8* }>, align 8
  %vararg_lifetime_bitcast6 = bitcast <{ i32, i8* }>* %vararg_buffer5 to i8*
  %vararg_buffer2 = alloca <{ i8* }>, align 8
  %vararg_lifetime_bitcast3 = bitcast <{ i8* }>* %vararg_buffer2 to i8*
  %vararg_buffer1 = alloca <{ i8*, i32 }>, align 8
  %vararg_lifetime_bitcast = bitcast <{ i8*, i32 }>* %vararg_buffer1 to i8*
  %muffin = alloca [117 x i8], align 1
  %cupcake = alloca [119 x i8], align 1
  %tmp = getelementptr [117 x i8], [117 x i8]* %muffin, i32 0, i32 0
  call void @llvm.lifetime.start(i64 117, i8* %tmp) #0
  %tmp1 = load i8*, i8** %argv, align 4
  call void @llvm.lifetime.start(i64 8, i8* %vararg_lifetime_bitcast)
  %vararg_ptr = getelementptr <{ i8*, i32 }>, <{ i8*, i32 }>* %vararg_buffer1, i32 0, i32 0
  store i8* %tmp1, i8** %vararg_ptr, align 4
  %vararg_ptr1 = getelementptr <{ i8*, i32 }>, <{ i8*, i32 }>* %vararg_buffer1, i32 0, i32 1
  store i32 %argc, i32* %vararg_ptr1, align 4
  %call = call i32 bitcast (i32 (i8*, i8*, i8*)* @sprintf to i32 (i8*, i8*, <{ i8*, i32 }>*)*)(i8* %tmp, i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str, i32 0, i32 0), <{ i8*, i32 }>* %vararg_buffer1) #0
  call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast)
  %tmp2 = load %struct._IO_FILE*, %struct._IO_FILE** bitcast ([4 x i8]* @stderr to %struct._IO_FILE**), align 4
  call void @llvm.lifetime.start(i64 4, i8* %vararg_lifetime_bitcast3)
  %vararg_ptr4 = getelementptr <{ i8* }>, <{ i8* }>* %vararg_buffer2, i32 0, i32 0
  store i8* %tmp, i8** %vararg_ptr4, align 4
  %call2 = call i32 bitcast (i32 (%struct._IO_FILE*, i8*, i8*)* @fprintf to i32 (%struct._IO_FILE*, i8*, <{ i8* }>*)*)(%struct._IO_FILE* %tmp2, i8* getelementptr inbounds ([33 x i8], [33 x i8]* @.str1, i32 0, i32 0), <{ i8* }>* %vararg_buffer2) #0
  call void @llvm.lifetime.end(i64 4, i8* %vararg_lifetime_bitcast3)
  call void @llvm.lifetime.end(i64 117, i8* %tmp) #0
  %tmp3 = getelementptr [119 x i8], [119 x i8]* %cupcake, i32 0, i32 0
  call void @llvm.lifetime.start(i64 119, i8* %tmp3) #0
  %tmp4 = load i8*, i8** %argv, align 4
  call void @llvm.lifetime.start(i64 8, i8* %vararg_lifetime_bitcast6)
  %vararg_ptr7 = getelementptr <{ i32, i8* }>, <{ i32, i8* }>* %vararg_buffer5, i32 0, i32 0
  store i32 %argc, i32* %vararg_ptr7, align 4
  %vararg_ptr8 = getelementptr <{ i32, i8* }>, <{ i32, i8* }>* %vararg_buffer5, i32 0, i32 1
  store i8* %tmp4, i8** %vararg_ptr8, align 4
  %call5 = call i32 bitcast (i32 (i8*, i8*, i8*)* @sprintf to i32 (i8*, i8*, <{ i32, i8* }>*)*)(i8* %tmp3, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @.str2, i32 0, i32 0), <{ i32, i8* }>* %vararg_buffer5) #0
  call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast6)
  call void @llvm.lifetime.start(i64 4, i8* %vararg_lifetime_bitcast10)
  %vararg_ptr11 = getelementptr <{ i8* }>, <{ i8* }>* %vararg_buffer0, i32 0, i32 0
  store i8* %tmp3, i8** %vararg_ptr11, align 4
  %call7 = call i32 bitcast (i32 (%struct._IO_FILE*, i8*, i8*)* @fprintf to i32 (%struct._IO_FILE*, i8*, <{ i8* }>*)*)(%struct._IO_FILE* %tmp2, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str3, i32 0, i32 0), <{ i8* }>* %vararg_buffer0) #0
  call void @llvm.lifetime.end(i64 4, i8* %vararg_lifetime_bitcast10)
  call void @llvm.lifetime.end(i64 119, i8* %tmp3) #0
  ret void
}

; CHECK: function _bar($argc,$argv) {
; CHECK-NOT: cupcake
; CHECK: STACKTOP = STACKTOP + 128|0;
; CHECK-NEXT: vararg_buffer0 =
; CHECK-NEXT: $muffin =
; CHECK-NOT: cupcake
; CHECK: }

; Function Attrs: nounwind
define void @bar(i32 %argc, i8** %argv) #0 {
entry:
  %vararg_buffer0 = alloca <{ i8* }>, align 8
  %vararg_lifetime_bitcast10 = bitcast <{ i8* }>* %vararg_buffer0 to i8*
  %vararg_buffer5 = alloca <{ i32, i8* }>, align 8
  %vararg_lifetime_bitcast6 = bitcast <{ i32, i8* }>* %vararg_buffer5 to i8*
  %vararg_buffer2 = alloca <{ i8* }>, align 8
  %vararg_lifetime_bitcast3 = bitcast <{ i8* }>* %vararg_buffer2 to i8*
  %vararg_buffer1 = alloca <{ i8*, i32 }>, align 8
  %vararg_lifetime_bitcast = bitcast <{ i8*, i32 }>* %vararg_buffer1 to i8*
  %muffin = alloca [117 x i8], align 1
  %cupcake = alloca [119 x i8], align 1
  %tmp = getelementptr [117 x i8], [117 x i8]* %muffin, i32 0, i32 0
  call void @llvm.lifetime.start(i64 117, i8* %tmp) #0
  %cmp = icmp eq i32 %argc, 39
  br i1 %cmp, label %if.end.thread, label %if.end

if.end.thread:                                    ; preds = %entry
  call void @llvm.lifetime.end(i64 117, i8* %tmp) #0
  %tmp1 = getelementptr [119 x i8], [119 x i8]* %cupcake, i32 0, i32 0
  call void @llvm.lifetime.start(i64 119, i8* %tmp1) #0
  %.pre = load %struct._IO_FILE*, %struct._IO_FILE** bitcast ([4 x i8]* @stderr to %struct._IO_FILE**), align 4
  br label %if.then4

if.end:                                           ; preds = %entry
  %tmp2 = load i8*, i8** %argv, align 4
  call void @llvm.lifetime.start(i64 8, i8* %vararg_lifetime_bitcast)
  %vararg_ptr = getelementptr <{ i8*, i32 }>, <{ i8*, i32 }>* %vararg_buffer1, i32 0, i32 0
  store i8* %tmp2, i8** %vararg_ptr, align 4
  %vararg_ptr1 = getelementptr <{ i8*, i32 }>, <{ i8*, i32 }>* %vararg_buffer1, i32 0, i32 1
  store i32 %argc, i32* %vararg_ptr1, align 4
  %call = call i32 bitcast (i32 (i8*, i8*, i8*)* @sprintf to i32 (i8*, i8*, <{ i8*, i32 }>*)*)(i8* %tmp, i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str, i32 0, i32 0), <{ i8*, i32 }>* %vararg_buffer1) #0
  call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast)
  %tmp3 = load %struct._IO_FILE*, %struct._IO_FILE** bitcast ([4 x i8]* @stderr to %struct._IO_FILE**), align 4
  call void @llvm.lifetime.start(i64 4, i8* %vararg_lifetime_bitcast3)
  %vararg_ptr4 = getelementptr <{ i8* }>, <{ i8* }>* %vararg_buffer2, i32 0, i32 0
  store i8* %tmp, i8** %vararg_ptr4, align 4
  %call2 = call i32 bitcast (i32 (%struct._IO_FILE*, i8*, i8*)* @fprintf to i32 (%struct._IO_FILE*, i8*, <{ i8* }>*)*)(%struct._IO_FILE* %tmp3, i8* getelementptr inbounds ([33 x i8], [33 x i8]* @.str1, i32 0, i32 0), <{ i8* }>* %vararg_buffer2) #0
  call void @llvm.lifetime.end(i64 4, i8* %vararg_lifetime_bitcast3)
  call void @llvm.lifetime.end(i64 117, i8* %tmp) #0
  %tmp4 = getelementptr [119 x i8], [119 x i8]* %cupcake, i32 0, i32 0
  call void @llvm.lifetime.start(i64 119, i8* %tmp4) #0
  %cmp3 = icmp eq i32 %argc, 45
  br i1 %cmp3, label %if.end10, label %if.then4

if.then4:                                         ; preds = %if.end, %if.end.thread
  %tmp5 = phi %struct._IO_FILE* [ %.pre, %if.end.thread ], [ %tmp3, %if.end ]
  %tmp6 = phi i8* [ %tmp1, %if.end.thread ], [ %tmp4, %if.end ]
  %tmp7 = load i8*, i8** %argv, align 4
  call void @llvm.lifetime.start(i64 8, i8* %vararg_lifetime_bitcast6)
  %vararg_ptr7 = getelementptr <{ i32, i8* }>, <{ i32, i8* }>* %vararg_buffer5, i32 0, i32 0
  store i32 %argc, i32* %vararg_ptr7, align 4
  %vararg_ptr8 = getelementptr <{ i32, i8* }>, <{ i32, i8* }>* %vararg_buffer5, i32 0, i32 1
  store i8* %tmp7, i8** %vararg_ptr8, align 4
  %call7 = call i32 bitcast (i32 (i8*, i8*, i8*)* @sprintf to i32 (i8*, i8*, <{ i32, i8* }>*)*)(i8* %tmp6, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @.str2, i32 0, i32 0), <{ i32, i8* }>* %vararg_buffer5) #0
  call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast6)
  call void @llvm.lifetime.start(i64 4, i8* %vararg_lifetime_bitcast10)
  %vararg_ptr11 = getelementptr <{ i8* }>, <{ i8* }>* %vararg_buffer0, i32 0, i32 0
  store i8* %tmp6, i8** %vararg_ptr11, align 4
  %call9 = call i32 bitcast (i32 (%struct._IO_FILE*, i8*, i8*)* @fprintf to i32 (%struct._IO_FILE*, i8*, <{ i8* }>*)*)(%struct._IO_FILE* %tmp5, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str3, i32 0, i32 0), <{ i8* }>* %vararg_buffer0) #0
  call void @llvm.lifetime.end(i64 4, i8* %vararg_lifetime_bitcast10)
  br label %if.end10

if.end10:                                         ; preds = %if.then4, %if.end
  %tmp8 = phi i8* [ %tmp4, %if.end ], [ %tmp6, %if.then4 ]
  call void @llvm.lifetime.end(i64 119, i8* %tmp8) #0
  ret void
}

; Function Attrs: nounwind
declare i32 @sprintf(i8*, i8*, i8*) #0

; Function Attrs: nounwind
declare i32 @fprintf(%struct._IO_FILE*, i8*, i8*) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

attributes #0 = { nounwind }
