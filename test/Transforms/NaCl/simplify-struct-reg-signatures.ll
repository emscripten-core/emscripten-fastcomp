; RUN: opt %s -simplify-struct-reg-signatures -S | FileCheck %s

declare i32 @__gxx_personality_v0(...)

%struct = type { i32, i32 }

%rec_struct = type {%rec_struct*}
%rec_problem_struct = type{void (%rec_problem_struct)*}
%rec_pair_1 = type {%rec_pair_2*}
%rec_pair_2 = type {%rec_pair_1*}
%rec_returning = type { %rec_returning (%rec_returning)* }
%direct_def = type { void(%struct)*, %struct }

; new type declarations:
; CHECK: %struct = type { i32, i32 }
; CHECK-NEXT: %rec_struct = type { %rec_struct* }
; CHECK-NEXT: %rec_problem_struct.simplified = type { void (%rec_problem_struct.simplified*)* }
; CHECK-NEXT: %rec_pair_1 = type { %rec_pair_2* }
; CHECK-NEXT: %rec_pair_2 = type { %rec_pair_1* }
; CHECK-NEXT: %rec_returning.simplified = type { void (%rec_returning.simplified*, %rec_returning.simplified*)* }
; CHECK-NEXT: %direct_def.simplified = type { void (%struct*)*, %struct }

; Leave intrinsics alone:
; CHECK: { i32, i1 } @llvm.uadd.with.overflow.i32(i32, i32)
declare { i32, i1 } @llvm.uadd.with.overflow.i32(i32, i32)

; CHECK-LABEL: define void @call_intrinsic()
define void @call_intrinsic() {
  %a = call { i32, i1 } @llvm.uadd.with.overflow.i32(i32 5, i32 5)
; CHECK-NEXT: %a = call { i32, i1 } @llvm.uadd.with.overflow.i32(i32 5, i32 5)
  ret void
}

; externs
declare void @extern_func(%struct)
declare %struct @struct_returning_extern(i32, %struct)

; verify that parameters are mapped correctly: single param, two, and combo
; with non-struct regs
; CHECK-NOT: declare void @extern_func(%struct)
; CHECK-NOT: declare %struct @struct_returning_extern(i32, %struct)
; CHECK-LABEL: declare void @extern_func(%struct* byval)
; CHECK-LABEL: declare void @struct_returning_extern(%struct* sret, i32, %struct* byval)

define void @main(%struct* byval %ptr) {
  %val = load %struct, %struct* %ptr
  call void @extern_func(%struct %val)
  ret void
}

define void @two_param_func(%struct %val1, %struct %val2) {
  call void @extern_func(%struct %val1)
  call void @extern_func(%struct %val2)
  ret void
}

; CHECK-LABEL: define void @two_param_func(%struct* byval %val1.ptr, %struct* byval %val2.ptr)
; CHECK-NOT: define void @two_param_func(%struct %val1, %struct %val2)

define i32 @another_func(i32 %a, %struct %str, i64 %b) {
  call void @two_param_func(%struct %str, %struct %str)
  call void @extern_func(%struct %str)
  ret i32 0
}

; CHECK-LABEL: define i32 @another_func(i32 %a, %struct* byval %str.ptr, i64 %b)
; CHECK: call void @two_param_func(%struct* byval %str.sreg.ptr, %struct* byval %str.sreg.ptr1)

define %struct @returns_struct(i32 %an_int, %struct %val) {
  %tmp = call %struct @struct_returning_extern(i32 %an_int, %struct %val)
  %tmp2 = invoke %struct @struct_returning_extern(i32 1, %struct %tmp)
    to label %Cont unwind label %Cleanup

Cont:
  ret %struct %tmp2
Cleanup:
  %exn = landingpad {i8*, i32} personality i32 (...)* @__gxx_personality_v0
    cleanup
  resume {i8*, i32} %exn
}

; verify return value and codegen
; CHECK-LABEL: define void @returns_struct(%struct* sret %retVal, i32 %an_int, %struct* byval %val.ptr)
; CHECK-NEXT:  %tmp2 = alloca %struct
; CHECK-NEXT:  %tmp.sreg.ptr = alloca %struct
; CHECK-NEXT:  %tmp = alloca %struct
; CHECK-NEXT:  %val.sreg.ptr = alloca %struct
; CHECK-NEXT:  %val.sreg = load %struct, %struct* %val.ptr
; CHECK-NEXT:  store %struct %val.sreg, %struct* %val.sreg.ptr
; CHECK-NEXT:  call void @struct_returning_extern(%struct* sret %tmp, i32 %an_int, %struct* byval %val.sreg.ptr)
; CHECK-NEXT:  %tmp.sreg = load %struct, %struct* %tmp
; CHECK-NEXT:  store %struct %tmp.sreg, %struct* %tmp.sreg.ptr
; CHECK-NEXT:  invoke void @struct_returning_extern(%struct* sret %tmp2, i32 1, %struct* byval %tmp.sreg.ptr)
; CHECK-NEXT:            to label %Cont unwind label %Cleanup
; CHECK-DAG:   Cont:
; CHECK-NEXT:    %tmp2.sreg = load %struct, %struct* %tmp2
; CHECK-NEXT:    store %struct %tmp2.sreg, %struct* %retVal
; CHECK-NEXT:    ret void
; CHECK-DAG:   Cleanup:
; CHECK-NEXT:    %exn = landingpad { i8*, i32 } personality i32 (...)* @__gxx_personality_v0
; CHECK-NEXT:            cleanup
; CHECK-NEXT:    resume { i8*, i32 } %exn

define i32 @lots_of_call_attrs() {
  %tmp.0 = insertvalue %struct undef, i32 1, 0
  %tmp.1 = insertvalue %struct %tmp.0, i32 2, 1
  %ret = tail call zeroext i32 @another_func(i32 1, %struct %tmp.1, i64 2) readonly
  ret i32 %ret
}

; verify attributes are copied
; CHECK_LABEL: @lots_of_call_attrs
; CHECK: %ret = tail call zeroext i32 @another_func(i32 1, %struct* byval %tmp.1.ptr, i64 2) #1
; CHECK-NEXT: ret i32 %ret

declare void @rec_struct_ok(%rec_struct*)
declare void @rec_struct_mod(%rec_struct)

; compliant recursive structs are kept as-is
; CHECK-LABEL: declare void @rec_struct_ok(%rec_struct*)
; CHECK-LABEL: declare void @rec_struct_mod(%rec_struct* byval)

define void @rec_call_sreg(%rec_problem_struct %r) {
  %tmp = extractvalue %rec_problem_struct %r, 0
  call void %tmp(%rec_problem_struct %r)
  ret void
}

; non-compliant structs are correctly mapped and calls are changed
; CHECK-LABEL: define void @rec_call_sreg(%rec_problem_struct.simplified* byval %r.ptr)
; CHECK: call void %tmp(%rec_problem_struct.simplified* byval %r.sreg.ptr)

declare void @pairs(%rec_pair_1)

define %rec_returning @rec_returning_fun(%rec_returning %str) {
  %tmp = extractvalue %rec_returning %str, 0
  %ret = call %rec_returning %tmp(%rec_returning %str)
  ret %rec_returning %ret
}

; pair structs
; CHECK-LABEL: declare void @pairs(%rec_pair_1* byval)
; CHECK-LABEL: define void @rec_returning_fun(%rec_returning.simplified* sret %retVal, %rec_returning.simplified* byval %str.ptr)
; CHECK-NEXT:   %ret = alloca %rec_returning.simplified
; CHECK-NEXT:   %str.sreg.ptr = alloca %rec_returning.simplified
; CHECK-NEXT:   %str.sreg = load %rec_returning.simplified, %rec_returning.simplified* %str.ptr
; CHECK-NEXT:   %tmp = extractvalue %rec_returning.simplified %str.sreg, 0
; CHECK-NEXT:   store %rec_returning.simplified %str.sreg, %rec_returning.simplified* %str.sreg.ptr
; CHECK-NEXT:   call void %tmp(%rec_returning.simplified* sret %ret, %rec_returning.simplified* byval %str.sreg.ptr)
; CHECK-NEXT:   %ret.sreg = load %rec_returning.simplified, %rec_returning.simplified* %ret
; CHECK-NEXT:   store %rec_returning.simplified %ret.sreg, %rec_returning.simplified* %retVal
; CHECK-NEXT:   ret void

define void @direct_caller(%direct_def %def) {
  %func = extractvalue %direct_def %def, 0
  %param = extractvalue %direct_def %def, 1
  call void %func(%struct %param)
  ret void
}

; CHECK-LABEL: define void @direct_caller(%direct_def.simplified* byval %def.ptr)
; CHECK-NEXT:  %param.ptr = alloca %struct
; CHECK-NEXT:  %def.sreg = load %direct_def.simplified, %direct_def.simplified* %def.ptr
; CHECK-NEXT:  %func = extractvalue %direct_def.simplified %def.sreg, 0
; CHECK-NEXT:  %param = extractvalue %direct_def.simplified %def.sreg, 1
; CHECK-NEXT:  store %struct %param, %struct* %param.ptr
; CHECK-NEXT:  call void %func(%struct* byval %param.ptr)
; CHECK-NEXT:  ret void

; vararg functions are converted correctly
declare void @vararg_ok(i32, ...)
; CHECK-LABEL: declare void @vararg_ok(i32, ...)

define void @vararg_problem(%rec_problem_struct %arg1, ...) {
  ; CHECK-LABEL: define void @vararg_problem(%rec_problem_struct.simplified* byval %arg1.ptr, ...)
   ret void
}

%vararg_fp_struct = type { i32, void (i32, ...)* }
declare void @vararg_fp_fct(%vararg_fp_struct %arg)
;CHECK-LABEL: declare void @vararg_fp_fct(%vararg_fp_struct* byval)

define void @call_vararg(%vararg_fp_struct %param1, ...) {
  %fptr = extractvalue %vararg_fp_struct %param1, 1
  call void (i32, ...) %fptr(i32 0, i32 1)
  ret void
}

; CHECK-LABEL: define void @call_vararg(%vararg_fp_struct* byval %param1.ptr, ...)
; CHECK-NEXT:  %param1.sreg = load %vararg_fp_struct, %vararg_fp_struct* %param1.ptr
; CHECK-NEXT:  %fptr = extractvalue %vararg_fp_struct %param1.sreg, 1
; CHECK-NEXT:  call void (i32, ...) %fptr(i32 0, i32 1)
; CHECK-NEXT:  ret void

%vararg_fp_problem_struct = type { void(%vararg_fp_problem_struct)* }
define void @vararg_fp_problem_call(%vararg_fp_problem_struct* byval %param) {
  %fct_ptr = getelementptr %vararg_fp_problem_struct, %vararg_fp_problem_struct* %param, i32 0, i32 0
  %fct = load void(%vararg_fp_problem_struct)*, void(%vararg_fp_problem_struct)** %fct_ptr
  %param_for_call = load %vararg_fp_problem_struct, %vararg_fp_problem_struct* %param
  call void %fct(%vararg_fp_problem_struct %param_for_call)
  ret void
}

; CHECK-LABEL: define void @vararg_fp_problem_call(%vararg_fp_problem_struct.simplified* byval %param)
; CHECK-NEXT:  %param_for_call.ptr = alloca %vararg_fp_problem_struct.simplified
; CHECK-NEXT:  %fct_ptr = getelementptr %vararg_fp_problem_struct.simplified, %vararg_fp_problem_struct.simplified* %param, i32 0, i32 0
; CHECK-NEXT:  %fct = load void (%vararg_fp_problem_struct.simplified*)*, void (%vararg_fp_problem_struct.simplified*)** %fct_ptr
; CHECK-NEXT:  %param_for_call = load %vararg_fp_problem_struct.simplified, %vararg_fp_problem_struct.simplified* %param
; CHECK-NEXT:  store %vararg_fp_problem_struct.simplified %param_for_call, %vararg_fp_problem_struct.simplified* %param_for_call.ptr
; CHECK-NEXT:  call void %fct(%vararg_fp_problem_struct.simplified* byval %param_for_call.ptr)
; CHECK-NEXT:  ret void

define void @call_with_array([4 x void(%struct)*] %fptrs, %struct %str) {
  %fptr = extractvalue [4 x void(%struct)*] %fptrs, 2
  call void %fptr(%struct %str)
  ret void
}

; CHECK-LABEL: define void @call_with_array([4 x void (%struct*)*]* byval %fptrs.ptr, %struct* byval %str.ptr)
; CHECK-NEXT:  %str.sreg.ptr = alloca %struct
; CHECK-NEXT:  %fptrs.sreg = load [4 x void (%struct*)*], [4 x void (%struct*)*]* %fptrs.ptr
; CHECK-NEXT:  %str.sreg = load %struct, %struct* %str.ptr
; CHECK-NEXT:  %fptr = extractvalue [4 x void (%struct*)*] %fptrs.sreg, 2
; CHECK-NEXT:  store %struct %str.sreg, %struct* %str.sreg.ptr
; CHECK-NEXT:  call void %fptr(%struct* byval %str.sreg.ptr)
; CHECK-NEXT:  ret void

define void @call_with_array_ptr([4 x void(%struct)*]* %fptrs, %struct %str) {
  %fptr_ptr = getelementptr [4 x void(%struct)*], [4 x void(%struct)*]* %fptrs, i32 0, i32 2
  %fptr = load void(%struct)*, void(%struct)** %fptr_ptr
  call void %fptr(%struct %str)
  ret void
}

; CHECK-LABEL: define void @call_with_array_ptr([4 x void (%struct*)*]* %fptrs, %struct* byval %str.ptr)
; CHECK-NEXT:  %str.sreg.ptr = alloca %struct
; CHECK-NEXT:  %str.sreg = load %struct, %struct* %str.ptr
; CHECK-NEXT:  %fptr_ptr = getelementptr [4 x void (%struct*)*], [4 x void (%struct*)*]* %fptrs, i32 0, i32 2
; CHECK-NEXT:  %fptr = load void (%struct*)*, void (%struct*)** %fptr_ptr
; CHECK-NEXT:  store %struct %str.sreg, %struct* %str.sreg.ptr
; CHECK-NEXT:  call void %fptr(%struct* byval %str.sreg.ptr)
; CHECK-NEXT:  ret void

define void @call_with_vector(<4 x void (%struct)*> %fptrs, %struct %str) {
  %fptr = extractelement <4 x void (%struct)*> %fptrs, i32 2
  call void %fptr(%struct %str)
  ret void
}

; CHECK-LABEL: define void @call_with_vector(<4 x void (%struct*)*> %fptrs, %struct* byval %str.ptr)
; CHECK-NEXT:  %str.sreg.ptr = alloca %struct
; CHECK-NEXT:  %str.sreg = load %struct, %struct* %str.ptr
; CHECK-NEXT:  %fptr = extractelement <4 x void (%struct*)*> %fptrs, i32 2
; CHECK-NEXT:  store %struct %str.sreg, %struct* %str.sreg.ptr
; CHECK-NEXT:  call void %fptr(%struct* byval %str.sreg.ptr)
; CHECK-NEXT:  ret void

define void @call_with_array_vect([4 x <2 x void(%struct)*>] %fptrs, %struct %str) {
  %vect = extractvalue [4 x <2 x void(%struct)*>] %fptrs, 2
  %fptr = extractelement <2 x void (%struct)*> %vect, i32 1
  call void %fptr(%struct %str)
  ret void
}

; CHECK-LABEL: define void @call_with_array_vect([4 x <2 x void (%struct*)*>]* byval %fptrs.ptr, %struct* byval %str.ptr)
; CHECK-NEXT:  %str.sreg.ptr = alloca %struct
; CHECK-NEXT:  %fptrs.sreg = load [4 x <2 x void (%struct*)*>], [4 x <2 x void (%struct*)*>]* %fptrs.ptr
; CHECK-NEXT:  %str.sreg = load %struct, %struct* %str.ptr
; CHECK-NEXT:  %vect = extractvalue [4 x <2 x void (%struct*)*>] %fptrs.sreg, 2
; CHECK-NEXT:  %fptr = extractelement <2 x void (%struct*)*> %vect, i32 1
; CHECK-NEXT:  store %struct %str.sreg, %struct* %str.sreg.ptr
; CHECK-NEXT:  call void %fptr(%struct* byval %str.sreg.ptr)
; CHECK-NEXT:  ret void

; this is at the end, corresponds to the call marked as readonly
; CHECK: attributes #1 = { readonly }