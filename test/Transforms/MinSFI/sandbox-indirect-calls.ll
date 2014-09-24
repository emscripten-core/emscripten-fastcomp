; RUN: opt %s -minsfi-sandbox-indirect-calls -S | FileCheck %s

!llvm.module.flags = !{!0}
!0 = metadata !{i32 1, metadata !"Debug Info Version", i32 1}

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

declare void @fn_v_v()
declare void @fn_v_i_not_addr_taken(i32)
declare void @fn_v_i_1(i32)
declare i32  @fn_i_i_not_addr_taken(i32)
declare i32  @fn_i_ii(i32, i32)
declare void @fn_v_i_2(i32)

declare void @foo_2ptr(i32, i32)

; CHECK-DAG: [[TAB_V_V:@__sfi_function_table[0-9]*]]  = internal constant [8 x void ()*]        [void ()* null,        void ()* @fn_v_v,     void ()* null,         void ()* null,            void ()* null,         void ()* null,        void ()* null,        void ()* null]
; CHECK-DAG: [[TAB_V_I:@__sfi_function_table[0-9]*]]  = internal constant [8 x void (i32)*]     [void (i32)* null,     void (i32)* null,     void (i32)* @fn_v_i_1, void (i32)* null,         void (i32)* @fn_v_i_2, void (i32)* null,     void (i32)* null,     void (i32)* null]
; CHECK-DAG: [[TAB_I_II:@__sfi_function_table[0-9]*]] = internal constant [8 x i32 (i32, i32)*] [i32 (i32, i32)* null, i32 (i32, i32)* null, i32 (i32, i32)* null,  i32 (i32, i32)* @fn_i_ii, i32 (i32, i32)* null,  i32 (i32, i32)* null, i32 (i32, i32)* null, i32 (i32, i32)* null]

@test_global_inits = 
  internal constant i32 ptrtoint (void (i32)* @fn_v_i_2 to i32)

; CHECK-DAG: @test_global_inits = internal constant i32 4

define void @test_direct_calls_not_replaced() {
  call void @fn_v_i_not_addr_taken(i32 5)
  call i32 @fn_i_i_not_addr_taken(i32 7)
  ret void
}

; CHECK-LABEL: define void @test_direct_calls_not_replaced() {
; CHECK-NEXT:    call void @fn_v_i_not_addr_taken(i32 5)
; CHECK-NEXT:    call i32 @fn_i_i_not_addr_taken(i32 7)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define void @test_ptr_consts_replaced() {
  call void @foo_2ptr(i32 ptrtoint (void (i32)* @fn_v_i_1 to i32),
                      i32 ptrtoint (void ()* @fn_v_v to i32))
  ret void
}

; CHECK-LABEL: define void @test_ptr_consts_replaced() {
; CHECK-NEXT:    call void @foo_2ptr(i32 2, i32 1)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define void @test_ptr_insts_replaced() {
  %ptr1 = ptrtoint void (i32)* @fn_v_i_2 to i32
  %ptr2 = ptrtoint i32 (i32, i32)* @fn_i_ii to i32
  call void @foo_2ptr(i32 %ptr1, i32 %ptr2)
  ret void
}

; CHECK-LABEL: define void @test_ptr_insts_replaced() {
; CHECK-NEXT:    call void @foo_2ptr(i32 4, i32 3)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define void @test_indirect_calls(i32 %index_v_i, i32 %index_i_ii) {
  %fn_v_i = inttoptr i32 %index_v_i to void (i32)*, !dbg !1
  call void %fn_v_i(i32 7), !dbg !2
  call void %fn_v_i(i32 9), !dbg !3
  %fn_i_ii = inttoptr i32 %index_i_ii to i32 (i32, i32)*, !dbg !4
  call i32 %fn_i_ii(i32 11, i32 13), !dbg !5
  ret void
}

; CHECK-LABEL: define void @test_indirect_calls(i32 %index_v_i, i32 %index_i_ii) {
; CHECK-NEXT:    %1 = and i32 %index_v_i, 7
; CHECK-NEXT:    %2 = getelementptr [8 x void (i32)*]* [[TAB_V_I]], i32 0, i32 %1
; CHECK-NEXT:    %3 = load void (i32)** %2, !dbg !1
; CHECK-NEXT:    call void %3(i32 7), !dbg !2
; CHECK-NEXT:    %4 = and i32 %index_v_i, 7
; CHECK-NEXT:    %5 = getelementptr [8 x void (i32)*]* [[TAB_V_I]], i32 0, i32 %4
; CHECK-NEXT:    %6 = load void (i32)** %5, !dbg !1
; CHECK-NEXT:    call void %6(i32 9), !dbg !3
; CHECK-NEXT:    %7 = and i32 %index_i_ii, 7
; CHECK-NEXT:    %8 = getelementptr [8 x i32 (i32, i32)*]* [[TAB_I_II]], i32 0, i32 %7
; CHECK-NEXT:    %9 = load i32 (i32, i32)** %8, !dbg !4
; CHECK-NEXT:    call i32 %9(i32 11, i32 13), !dbg !5
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define float @test_call_without_a_table(i32 %index) {
  %fn = inttoptr i32 %index to float (float)*
  %ret = call float %fn(float 0.000000e+00)
  ret float %ret
}

; CHECK-LABEL: define float @test_call_without_a_table(i32 %index) {
; CHECK-NEXT:    call void @llvm.trap()
; CHECK-NEXT:    %ret = call float null(float 0.000000e+00)
; CHECK-NEXT:    ret float %ret
; CHECK-NEXT:  }

!1 = metadata !{i32 138, i32 0, metadata !1, null}
!2 = metadata !{i32 142, i32 0, metadata !2, null}
!3 = metadata !{i32 144, i32 0, metadata !3, null}
!4 = metadata !{i32 144, i32 0, metadata !4, null}
!5 = metadata !{i32 144, i32 0, metadata !5, null}
