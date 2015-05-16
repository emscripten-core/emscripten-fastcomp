; RUN: llc < %s | FileCheck %s

; Lifetime intrinsics are typically just referencing a single alloca, but
; sometimes PRE decides to totally optimize a redundant bitcast and insert
; phis. We need to look through the phis. In the code below, l_1565.i has
; an overlapping lifetime with l_766.i which is only visible if we can
; see through phis.

; CHECK: $vararg_buffer3 = sp;
; CHECK: $l_1565$i = sp + 16|0;
; CHECK: $l_766$i = sp + 12|0;

target datalayout = "e-p:32:32-i64:64-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

@g_15 = external hidden global [4 x i8], align 4
@g_285 = external hidden global [4 x i8], align 4
@g_423 = external hidden global i32, align 4
@g_779 = external hidden global [4 x i8], align 4
@g_784 = external hidden global [4 x i8], align 4
@.str = external hidden unnamed_addr constant [25 x i8], align 1
@.str1 = external hidden unnamed_addr constant [25 x i8], align 1
@.str2 = external hidden unnamed_addr constant [15 x i8], align 1
@.str3 = external hidden unnamed_addr constant [8 x i8], align 1
@__func__._Z6func_6v = external hidden unnamed_addr constant [7 x i8], align 1

; Function Attrs: nounwind
declare i32 @printf(i8* nocapture readonly, i8* noalias) #0

; Function Attrs: noreturn
declare void @__assert_fail(i8*, i8*, i32, i8*) #1

define void @test() {
entry:
  %vararg_buffer3 = alloca <{ i32*, i32**, i32* }>, align 8
  %vararg_lifetime_bitcast4 = bitcast <{ i32*, i32**, i32* }>* %vararg_buffer3 to i8*
  %vararg_buffer = alloca <{ i32*, i32**, i32* }>, align 8
  %vararg_lifetime_bitcast = bitcast <{ i32*, i32**, i32* }>* %vararg_buffer to i8*
  %l_767.i.i = alloca i32, align 4
  %l_1565.i = alloca i32*, align 4
  %l_767.i = alloca i32, align 4
  %l_766.i = alloca [1 x i16*], align 4
  %0 = load i32, i32* bitcast ([4 x i8]* @g_15 to i32*), align 4
  %tobool = icmp eq i32 %0, 0
  br i1 %tobool, label %if.then, label %entry.if.end_crit_edge

entry.if.end_crit_edge:                           ; preds = %entry
  %.pre = bitcast [1 x i16*]* %l_766.i to i8*
  %.pre1 = getelementptr inbounds [1 x i16*], [1 x i16*]* %l_766.i, i32 0, i32 0
  br label %if.end

if.then:                                          ; preds = %entry
  %1 = bitcast i32* %l_767.i to i8*
  call void @llvm.lifetime.start(i64 4, i8* %1)
  %2 = bitcast [1 x i16*]* %l_766.i to i8*
  call void @llvm.lifetime.start(i64 4, i8* %2)
  store i32 -1407759351, i32* %l_767.i, align 4
  %3 = getelementptr inbounds [1 x i16*], [1 x i16*]* %l_766.i, i32 0, i32 0
  store i16* null, i16** %3, align 4
  br label %for.body.i

for.body.i:                                       ; preds = %for.body.i, %if.then
  %l_82.02.i = phi i32 [ 0, %if.then ], [ %inc.i, %for.body.i ]
  %4 = load i32**, i32*** bitcast (i32* @g_423 to i32***), align 4
  store i32* %l_767.i, i32** %4, align 4
  store i16** %3, i16*** bitcast ([4 x i8]* @g_779 to i16***), align 4
  %inc.i = add i32 %l_82.02.i, 1
  %exitcond.i = icmp eq i32 %inc.i, 27
  br i1 %exitcond.i, label %_Z7func_34v.exit, label %for.body.i

_Z7func_34v.exit:                                 ; preds = %for.body.i
  call void @llvm.lifetime.end(i64 4, i8* %1)
  call void @llvm.lifetime.end(i64 4, i8* %2)
  %5 = load i32**, i32*** bitcast (i32* @g_423 to i32***), align 4
  store i32* bitcast ([4 x i8]* @g_285 to i32*), i32** %5, align 4
  br label %if.end

if.end:                                           ; preds = %_Z7func_34v.exit, %entry.if.end_crit_edge
  %.pre-phi2 = phi i16** [ %.pre1, %entry.if.end_crit_edge ], [ %3, %_Z7func_34v.exit ]
  %.pre-phi = phi i8* [ %.pre, %entry.if.end_crit_edge ], [ %2, %_Z7func_34v.exit ]
  %6 = bitcast i32** %l_1565.i to i8*
  call void @llvm.lifetime.start(i64 4, i8* %6)
  store i32* bitcast ([4 x i8]* @g_784 to i32*), i32** %l_1565.i, align 4
  call void @llvm.lifetime.start(i64 12, i8* %vararg_lifetime_bitcast)
  %vararg_ptr = getelementptr <{ i32*, i32**, i32* }>, <{ i32*, i32**, i32* }>* %vararg_buffer, i32 0, i32 0
  store i32* bitcast ([4 x i8]* @g_784 to i32*), i32** %vararg_ptr, align 4
  %vararg_ptr1 = getelementptr <{ i32*, i32**, i32* }>, <{ i32*, i32**, i32* }>* %vararg_buffer, i32 0, i32 1
  store i32** %l_1565.i, i32*** %vararg_ptr1, align 4
  %vararg_ptr2 = getelementptr <{ i32*, i32**, i32* }>, <{ i32*, i32**, i32* }>* %vararg_buffer, i32 0, i32 2
  store i32* bitcast ([4 x i8]* @g_784 to i32*), i32** %vararg_ptr2, align 4
  %call.i = call i32 bitcast (i32 (i8*, i8*)* @printf to i32 (i8*, <{ i32*, i32**, i32* }>*)*)(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @.str, i32 0, i32 0), <{ i32*, i32**, i32* }>* %vararg_buffer)
  call void @llvm.lifetime.end(i64 12, i8* %vararg_lifetime_bitcast)
  %7 = bitcast i32* %l_767.i.i to i8*
  call void @llvm.lifetime.start(i64 4, i8* %7)
  call void @llvm.lifetime.start(i64 4, i8* %.pre-phi)
  store i32 -1407759351, i32* %l_767.i.i, align 4
  store i16* null, i16** %.pre-phi2, align 4
  br label %for.body.i.i

for.body.i.i:                                     ; preds = %for.body.i.i, %if.end
  %l_82.02.i.i = phi i32 [ 0, %if.end ], [ %inc.i.i, %for.body.i.i ]
  %8 = load i32**, i32*** bitcast (i32* @g_423 to i32***), align 4
  store i32* %l_767.i.i, i32** %8, align 4
  store i16** %.pre-phi2, i16*** bitcast ([4 x i8]* @g_779 to i16***), align 4
  %inc.i.i = add i32 %l_82.02.i.i, 1
  %exitcond.i.i = icmp eq i32 %inc.i.i, 27
  br i1 %exitcond.i.i, label %_Z7func_34v.exit.i, label %for.body.i.i

_Z7func_34v.exit.i:                               ; preds = %for.body.i.i
  call void @llvm.lifetime.end(i64 4, i8* %7)
  call void @llvm.lifetime.end(i64 4, i8* %.pre-phi)
  %9 = load i32*, i32** %l_1565.i, align 4
  call void @llvm.lifetime.start(i64 12, i8* %vararg_lifetime_bitcast4)
  %vararg_ptr5 = getelementptr <{ i32*, i32**, i32* }>, <{ i32*, i32**, i32* }>* %vararg_buffer3, i32 0, i32 0
  store i32* %9, i32** %vararg_ptr5, align 4
  %vararg_ptr6 = getelementptr <{ i32*, i32**, i32* }>, <{ i32*, i32**, i32* }>* %vararg_buffer3, i32 0, i32 1
  store i32** %l_1565.i, i32*** %vararg_ptr6, align 4
  %vararg_ptr7 = getelementptr <{ i32*, i32**, i32* }>, <{ i32*, i32**, i32* }>* %vararg_buffer3, i32 0, i32 2
  store i32* bitcast ([4 x i8]* @g_784 to i32*), i32** %vararg_ptr7, align 4
  %call1.i = call i32 bitcast (i32 (i8*, i8*)* @printf to i32 (i8*, <{ i32*, i32**, i32* }>*)*)(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @.str1, i32 0, i32 0), <{ i32*, i32**, i32* }>* %vararg_buffer3)
  call void @llvm.lifetime.end(i64 12, i8* %vararg_lifetime_bitcast4)
  %10 = load i32*, i32** %l_1565.i, align 4
  %cmp.i = icmp eq i32* %10, bitcast ([4 x i8]* @g_784 to i32*)
  br i1 %cmp.i, label %_Z6func_6v.exit, label %lor.rhs.i

lor.rhs.i:                                        ; preds = %_Z7func_34v.exit.i
  call void @__assert_fail(i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str2, i32 0, i32 0), i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str3, i32 0, i32 0), i32 33, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @__func__._Z6func_6v, i32 0, i32 0)) #1
  unreachable

_Z6func_6v.exit:                                  ; preds = %_Z7func_34v.exit.i
  call void @llvm.lifetime.end(i64 4, i8* %6)
  ret void
}

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

attributes #0 = { nounwind }
attributes #1 = { noreturn }
