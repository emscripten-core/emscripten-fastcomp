; RUN: opt < %s -expand-varargs -S | FileCheck %s

target datalayout = "p:32:32:32"

%va_list = type i8*

declare void @llvm.va_start(i8*)
declare void @llvm.va_end(i8*)
declare void @llvm.va_copy(i8*, i8*)

declare i32 @outside_func(i32 %arg, %va_list* %args)

define i32 @varargs_func(i32 %arg, ...) {
  %arglist_alloc = alloca %va_list
  %arglist = bitcast %va_list* %arglist_alloc to i8*

  call void @llvm.va_start(i8* %arglist)
  %result = call i32 @outside_func(i32 %arg, %va_list* %arglist_alloc)
  call void @llvm.va_end(i8* %arglist)
  ret i32 %result
}
; CHECK-LABEL: define i32 @varargs_func(i32 %arg, i8* noalias %varargs) {
; CHECK-NEXT: %arglist_alloc = alloca i8*
; CHECK-NEXT: %arglist = bitcast i8** %arglist_alloc to i8*
; CHECK-NEXT: %arglist1 = bitcast i8* %arglist to i8**
; CHECK-NEXT: store i8* %varargs, i8** %arglist1
; CHECK-NEXT: %result = call i32 @outside_func(i32 %arg, i8** %arglist_alloc)
; CHECK-NEXT: ret i32 %result


; Obtain each argument in the va_list according to its type (known from fmt).
; This function ensures that each argument is loaded with the same alignment as
; if it were inside a struct: this is how the caller passed the arguments.
;
; Note that alignof is represented as a GEP off of nullptr to the second element
; of a struct with { i1, types_whose_alignment_is_desired }.
define void @varargs_func_2(i8* nocapture %o8, i8* nocapture readonly %fmt, ...) {
; CHECK-LABEL: @varargs_func_2(
entry:
  %o16 = bitcast i8* %o8 to i16*
  %o32 = bitcast i8* %o8 to i32*
  %o64 = bitcast i8* %o8 to i64*
  %ofloat = bitcast i8* %o8 to float*
  %odouble = bitcast i8* %o8 to double*

  %arglist_alloc = alloca [4 x i32], align 4
  %arglist = getelementptr inbounds [4 x i32], [4 x i32]* %arglist_alloc, i32 0, i32 0
  %arglist.i8 = bitcast [4 x i32]* %arglist_alloc to i8*
  call void @llvm.va_start(i8* %arglist.i8)
  br label %start

start:
  %idx = phi i32 [ 0, %entry ], [ %inc, %next ]
  %fmt.gep = getelementptr inbounds i8, i8* %fmt, i32 %idx
  %arg.type = load i8, i8* %fmt.gep
  switch i8 %arg.type, label %next [
    i8 0, label %done
    i8 1, label %type.i8
    i8 2, label %type.i16
    i8 3, label %type.i32
    i8 4, label %type.i64
    i8 5, label %type.float
    i8 6, label %type.double
  ]

type.i8: ; CHECK: type.i8:
  %i8 = va_arg i32* %arglist, i8
  store i8 %i8, i8* %o8
  br label %next
; CHECK-NEXT: %arglist1 = bitcast i32* %arglist to i8**
; CHECK-NEXT: %arglist_current = load i8*, i8** %arglist1
; CHECK-NEXT: %[[P2I:[0-9]+]] = ptrtoint i8* %arglist_current to i32
; %A8 = (uintptr_t)Addr + Alignment - 1
; CHECK-NEXT: %[[A8:[0-9]+]] = add nuw i32 %[[P2I]], sub nuw (i32 ptrtoint (i8* getelementptr ({ i1, i8 }, { i1, i8 }* null, i64 0, i32 1) to i32), i32 1)
; %B8 = %1 & ~(uintptr_t)(Alignment - 1)
; CHECK-NEXT: %[[B8:[0-9]+]] = and i32 %[[A8]], xor (i32 sub nuw (i32 ptrtoint (i8* getelementptr ({ i1, i8 }, { i1, i8 }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %[[C8:[0-9]+]] = inttoptr i32 %[[B8]] to i8*
; CHECK-NEXT: %i8 = load i8, i8* %[[C8]]
; CHECK-NEXT: %arglist_next = getelementptr inbounds i8, i8* %[[C8]], i32 1
; CHECK-NEXT: store i8* %arglist_next, i8** %arglist1
; CHECK-NEXT: store i8 %i8, i8* %o8
; CHECK-NEXT: br label %next

type.i16: ; CHECK: type.i16:
  %i16 = va_arg i32* %arglist, i16
  store i16 %i16, i16* %o16
  br label %next
; CHECK:      %[[A16:[0-9]+]] = add nuw i32 %4, sub nuw (i32 ptrtoint (i16* getelementptr ({ i1, i16 }, { i1, i16 }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %[[B16:[0-9]+]] = and i32 %[[A16]], xor (i32 sub nuw (i32 ptrtoint (i16* getelementptr ({ i1, i16 }, { i1, i16 }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %[[C16:[0-9]+]] = inttoptr i32 %[[B16]] to i16*
; CHECK-NEXT: %i16 = load i16, i16* %[[C16]]

type.i32: ; CHECK: type.i32:
  %i32 = va_arg i32* %arglist, i32
  store i32 %i32, i32* %o32
  br label %next
; CHECK:      %[[A32:[0-9]+]] = add nuw i32 %8, sub nuw (i32 ptrtoint (i32* getelementptr ({ i1, i32 }, { i1, i32 }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %[[B32:[0-9]+]] = and i32 %[[A32]], xor (i32 sub nuw (i32 ptrtoint (i32* getelementptr ({ i1, i32 }, { i1, i32 }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %[[C32:[0-9]+]] = inttoptr i32 %[[B32]] to i32*
; CHECK-NEXT: %i32 = load i32, i32* %[[C32]]

type.i64: ; CHECK: type.i64:
  %i64 = va_arg i32* %arglist, i64
  store i64 %i64, i64* %o64
  br label %next
; CHECK:      %[[A64:[0-9]+]] = add nuw i32 %12, sub nuw (i32 ptrtoint (i64* getelementptr ({ i1, i64 }, { i1, i64 }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %[[B64:[0-9]+]] = and i32 %[[A64]], xor (i32 sub nuw (i32 ptrtoint (i64* getelementptr ({ i1, i64 }, { i1, i64 }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %[[C64:[0-9]+]] = inttoptr i32 %[[B64]] to i64*
; CHECK-NEXT: %i64 = load i64, i64* %[[C64]]

type.float: ; CHECK: type.float:
  %float = va_arg i32* %arglist, float
  store float %float, float* %ofloat
  br label %next
; CHECK:      %[[AF:[0-9]+]] = add nuw i32 %16, sub nuw (i32 ptrtoint (float* getelementptr ({ i1, float }, { i1, float }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %[[BF:[0-9]+]] = and i32 %[[AF]], xor (i32 sub nuw (i32 ptrtoint (float* getelementptr ({ i1, float }, { i1, float }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %[[CF:[0-9]+]] = inttoptr i32 %[[BF]] to float*
; CHECK-NEXT: %float = load float, float* %[[CF]]

type.double: ; CHECK: type.double:
  %double = va_arg i32* %arglist, double
  store double %double, double* %odouble
  br label %next
; CHECK:      %[[AD:[0-9]+]] = add nuw i32 %20, sub nuw (i32 ptrtoint (double* getelementptr ({ i1, double }, { i1, double }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %[[BD:[0-9]+]] = and i32 %[[AD]], xor (i32 sub nuw (i32 ptrtoint (double* getelementptr ({ i1, double }, { i1, double }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %[[CD:[0-9]+]] = inttoptr i32 %[[BD]] to double*
; CHECK-NEXT: %double = load double, double* %[[CD]]

next:
  %inc = add i32 %idx, 1
  br label %start

done:
  call void @llvm.va_end(i8* %arglist.i8)
  ret void
}


define i32 @varargs_call1() {
  %result = call i32 (i32, ...) @varargs_func(i32 111, i64 222, i32 333, double 4.0)
  ret i32 %result
}
; CHECK-LABEL: @varargs_call1(
; CHECK-NEXT: %vararg_buffer = alloca { i64, i32, double }
; CHECK-NEXT: %vararg_lifetime_bitcast = bitcast { i64, i32, double }* %vararg_buffer to i8*
; CHECK-NEXT: call void @llvm.lifetime.start(i64 24, i8* %vararg_lifetime_bitcast)
; CHECK-NEXT: %vararg_ptr = getelementptr inbounds { i64, i32, double }, { i64, i32, double }* %vararg_buffer, i32 0, i32 0
; CHECK-NEXT: store i64 222, i64* %vararg_ptr
; CHECK-NEXT: %vararg_ptr1 = getelementptr inbounds { i64, i32, double }, { i64, i32, double }* %vararg_buffer, i32 0, i32 1
; CHECK-NEXT: store i32 333, i32* %vararg_ptr1
; CHECK-NEXT: %vararg_ptr2 = getelementptr inbounds { i64, i32, double }, { i64, i32, double }* %vararg_buffer, i32 0, i32 2
; CHECK-NEXT: store double 4.{{0*}}e+00, double* %vararg_ptr2
; CHECK-NEXT: %result = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64, i32, double }*)*)(i32 111, { i64, i32, double }* %vararg_buffer)
; CHECK-NEXT: call void @llvm.lifetime.end(i64 24, i8* %vararg_lifetime_bitcast)
; CHECK-NEXT: ret i32 %result


; Check that the pass works when there are no variable arguments.
define i32 @call_with_zero_varargs() {
  %result = call i32 (i32, ...) @varargs_func(i32 111)
  ret i32 %result
}
; CHECK-LABEL: @call_with_zero_varargs(
; We have a dummy i32 field to deal with buggy programs:
; CHECK-NEXT: %vararg_buffer = alloca { i32 }
; CHECK-NEXT: %vararg_lifetime_bitcast = bitcast { i32 }* %vararg_buffer to i8*
; CHECK-NEXT: call void @llvm.lifetime.start(i64 4, i8* %vararg_lifetime_bitcast)
; CHECK-NEXT: %result = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i32 }*)*)(i32 111, { i32 }* %vararg_buffer)
; CHECK-NEXT: call void @llvm.lifetime.end(i64 4, i8* %vararg_lifetime_bitcast)
; CHECK-NEXT: ret i32 %result


; Check that "invoke" instructions are expanded out too.
define i32 @varargs_invoke() {
  %result = invoke i32 (i32, ...)* @varargs_func(i32 111, i64 222)
      to label %cont unwind label %lpad
cont:
  ret i32 %result
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i32 0
}
; CHECK-LABEL: @varargs_invoke(
; CHECK: call void @llvm.lifetime.start(i64 8, i8* %vararg_lifetime_bitcast)
; CHECK: %result = invoke i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64 }*)*)(i32 111, { i64 }* %vararg_buffer)
; CHECK-NEXT:    to label %cont unwind label %lpad
; CHECK: cont:
; CHECK-NEXT: call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast)
; CHECK: lpad:
; CHECK: call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast)


define void @varargs_multiple_calls() {
  %call1 = call i32 (i32, ...) @varargs_func(i32 11, i64 22, i32 33)
  %call2 = call i32 (i32, ...) @varargs_func(i32 44, i64 55, i32 66)
  ret void
}
; CHECK-LABEL: @varargs_multiple_calls(
; The added allocas should appear at the start of the function.
; CHECK: %vararg_buffer{{.*}} = alloca { i64, i32 }
; CHECK: %vararg_buffer{{.*}} = alloca { i64, i32 }
; CHECK: %call1 = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64, i32 }*)*)(i32 11, { i64, i32 }* %vararg_buffer{{.*}})
; CHECK: %call2 = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64, i32 }*)*)(i32 44, { i64, i32 }* %vararg_buffer{{.*}})



define i32 @va_arg_i32(i8* %arglist) {
  %result = va_arg i8* %arglist, i32
  ret i32 %result
}
; CHECK-LABEL: define i32 @va_arg_i32(i8* %arglist) {
; CHECK-NEXT: %arglist1 = bitcast i8* %arglist to i32**
; CHECK-NEXT: %arglist_current = load i32*, i32** %arglist1
; CHECK-NEXT: %1 = ptrtoint i32* %arglist_current to i32
; CHECK-NEXT: %2 = add nuw i32 %1, sub nuw (i32 ptrtoint (i32* getelementptr ({ i1, i32 }, { i1, i32 }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %3 = and i32 %2, xor (i32 sub nuw (i32 ptrtoint (i32* getelementptr ({ i1, i32 }, { i1, i32 }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %4 = inttoptr i32 %3 to i32*
; CHECK-NEXT: %result = load i32, i32* %4
; CHECK-NEXT: %arglist_next = getelementptr inbounds i32, i32* %4, i32 1
; CHECK-NEXT: store i32* %arglist_next, i32** %arglist1
; CHECK-NEXT: ret i32 %result


define i64 @va_arg_i64(i8* %arglist) {
  %result = va_arg i8* %arglist, i64
  ret i64 %result
}
; CHECK-LABEL: define i64 @va_arg_i64(i8* %arglist) {
; CHECK-NEXT: %arglist1 = bitcast i8* %arglist to i64**
; CHECK-NEXT: %arglist_current = load i64*, i64** %arglist1
; CHECK-NEXT: %1 = ptrtoint i64* %arglist_current to i32
; CHECK-NEXT: %2 = add nuw i32 %1, sub nuw (i32 ptrtoint (i64* getelementptr ({ i1, i64 }, { i1, i64 }* null, i64 0, i32 1) to i32), i32 1)
; CHECK-NEXT: %3 = and i32 %2, xor (i32 sub nuw (i32 ptrtoint (i64* getelementptr ({ i1, i64 }, { i1, i64 }* null, i64 0, i32 1) to i32), i32 1), i32 -1)
; CHECK-NEXT: %4 = inttoptr i32 %3 to i64*
; CHECK-NEXT: %result = load i64, i64* %4
; CHECK-NEXT: %arglist_next = getelementptr inbounds i64, i64* %4, i32 1
; CHECK-NEXT: store i64* %arglist_next, i64** %arglist1
; CHECK-NEXT: ret i64 %result


define void @do_va_copy(i8* %dest, i8* %src) {
  call void @llvm.va_copy(i8* %dest, i8* %src)
  ret void
}
; CHECK-LABEL: define void @do_va_copy(
; CHECK-NEXT: %vacopy_src = bitcast i8* %src to i8**
; CHECK-NEXT: %vacopy_dest = bitcast i8* %dest to i8**
; CHECK-NEXT: %vacopy_currentptr = load i8*, i8** %vacopy_src
; CHECK-NEXT: store i8* %vacopy_currentptr, i8** %vacopy_dest
; CHECK-NEXT: ret void
