; RUN: opt %s -pnacl-sjlj-eh -S | FileCheck %s

; Example std::type_info objects.
@exc_typeid1 = external global i8
@exc_typeid2 = external global i8
@exc_typeid3 = external global i8

; This must be declared for "-pnacl-sjlj-eh" to work.
@__pnacl_eh_stack = external thread_local global i8*

declare i32 @llvm.eh.typeid.for(i8*)

declare void @external_func()


@__pnacl_eh_type_table = external global i8*
@__pnacl_eh_action_table = external global i8*
@__pnacl_eh_filter_table = external global i8*

; CHECK: %action_table_entry = type { i32, i32 }

; CHECK: @__pnacl_eh_type_table = internal constant [4 x i8*] [i8* @exc_typeid1, i8* @exc_typeid2, i8* @exc_typeid3, i8* null]

; CHECK: @__pnacl_eh_action_table = internal constant [7 x %action_table_entry] [%action_table_entry { i32 3, i32 0 }, %action_table_entry { i32 2, i32 1 }, %action_table_entry { i32 1, i32 2 }, %action_table_entry { i32 -1, i32 0 }, %action_table_entry { i32 -2, i32 0 }, %action_table_entry { i32 4, i32 0 }, %action_table_entry zeroinitializer]

; CHECK: @__pnacl_eh_filter_table = internal constant [5 x i32] [i32 0, i32 2, i32 3, i32 1, i32 0]


; Exception type pointers are allocated IDs which specify the index
; into __pnacl_eh_type_table where the type may be found.
define void @test_eh_typeid(i32 %arg) {
  %id1 = call i32 @llvm.eh.typeid.for(i8* @exc_typeid1)
  %id2 = call i32 @llvm.eh.typeid.for(i8* @exc_typeid2)
  %id3 = call i32 @llvm.eh.typeid.for(i8* @exc_typeid3)
  %cmp1 = icmp eq i32 %arg, %id1
  %cmp2 = icmp eq i32 %arg, %id2
  %cmp3 = icmp eq i32 %arg, %id3
  ret void
}
; CHECK: define void @test_eh_typeid
; CHECK-NEXT: %cmp1 = icmp eq i32 %arg, 1
; CHECK-NEXT: %cmp2 = icmp eq i32 %arg, 2
; CHECK-NEXT: %cmp3 = icmp eq i32 %arg, 3
; CHECK-NEXT: ret void


define void @test_single_catch_clause() {
  invoke void @external_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null
      catch i8* @exc_typeid3
  ret void
}
; CHECK: define void @test_single_catch_clause
; CHECK: store i32 1, i32* %exc_info_ptr


define void @test_multiple_catch_clauses() {
  invoke void @external_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null
      catch i8* @exc_typeid1
      catch i8* @exc_typeid2
      catch i8* @exc_typeid3
  ret void
}
; CHECK: define void @test_multiple_catch_clauses
; CHECK: store i32 3, i32* %exc_info_ptr


define void @test_empty_filter_clause() {
  invoke void @external_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null
      filter [0 x i8*] zeroinitializer
  ret void
}
; CHECK: define void @test_empty_filter_clause
; CHECK: store i32 4, i32* %exc_info_ptr


define void @test_filter_clause() {
  invoke void @external_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null
      filter [3 x i8*] [i8* @exc_typeid2,
                        i8* @exc_typeid3,
                        i8* @exc_typeid1]
  ret void
}
; CHECK: define void @test_filter_clause
; CHECK: store i32 5, i32* %exc_info_ptr


; "catch i8* null" means that any C++ exception matches.
define void @test_catch_all_clause() {
  invoke void @external_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null
      catch i8* null
  ret void
}
; CHECK: define void @test_catch_all_clause
; CHECK: store i32 6, i32* %exc_info_ptr


define void @test_cleanup_clause() {
  invoke void @external_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null
      cleanup
  ret void
}
; CHECK: define void @test_cleanup_clause
; CHECK: store i32 7, i32* %exc_info_ptr
