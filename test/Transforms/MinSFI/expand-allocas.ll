; RUN: opt %s -minsfi-expand-allocas -S | FileCheck %s

!llvm.module.flags = !{!0}
!0 = metadata !{i32 1, metadata !"Debug Info Version", i32 1}

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

; Check that the stack pointer global variable is created. This does not check 
; the initial value of the stack ptr.
; CHECK: @__sfi_stack_ptr = internal global i32

declare i8* @llvm.stacksave()
declare void @llvm.stackrestore(i8* %ptr)

declare void @foo()

define i32 @test_no_alloca(i1 %cond) {
  br i1 %cond, label %IfOne, label %IfNotOne
IfOne:
  call void @foo()
  ret i32 2
IfNotOne:
  ret i32 3
}

; CHECK-LABEL: define i32 @test_no_alloca(i1 %cond) {
; CHECK-NEXT:    br i1 %cond, label %IfOne, label %IfNotOne
; CHECK:       IfOne:
; CHECK-NEXT:    call void @foo()
; CHECK-NEXT:    ret i32 2
; CHECK:       IfNotOne:
; CHECK-NEXT:    ret i32 3
; CHECK-NEXT:  }

define i8* @test_const_alloca() {
  %ptr = alloca i8, i32 9, !dbg !1
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_const_alloca() {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, 9
; CHECK-NEXT:    %ptr = inttoptr i32 %1 to i8*, !dbg !1
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_const_alloca_align() {
  %ptr = alloca i8, i32 9, align 536870912  ; biggest possible alignment
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_const_alloca_align() {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, 9
; CHECK-NEXT:    %2 = and i32 %1, -536870912
; CHECK-NEXT:    %ptr = inttoptr i32 %2 to i8*
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_variable_length_alloca(i32 %size) {
  %ptr = alloca i8, i32 %size, !dbg !1
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_variable_length_alloca(i32 %size) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, %size
; CHECK-NEXT:    %ptr = inttoptr i32 %1 to i8*, !dbg !1
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_variable_length_alloca_align(i32 %size) {
  %ptr = alloca i8, i32 %size, align 32
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_variable_length_alloca_align(i32 %size) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, %size
; CHECK-NEXT:    %2 = and i32 %1, -32
; CHECK-NEXT:    %ptr = inttoptr i32 %2 to i8*
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_const_after_const_alloca() {
  %ptr1 = alloca i8, i32 4
  %ptr2 = alloca i8, i32 8
  ret i8* %ptr2
}

; CHECK-LABEL: define i8* @test_const_after_const_alloca() {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, 4
; CHECK-NEXT:    %ptr1 = inttoptr i32 %1 to i8*
; CHECK-NEXT:    %2 = sub i32 %1, 8
; CHECK-NEXT:    %ptr2 = inttoptr i32 %2 to i8*
; CHECK-NEXT:    ret i8* %ptr2
; CHECK-NEXT:  }

define i8* @test_const_after_variable_alloca(i32 %size) {
  %ptr1 = alloca i8, i32 %size
  %ptr2 = alloca i8, i32 19
  ret i8* %ptr2
}

; CHECK-LABEL: define i8* @test_const_after_variable_alloca(i32 %size) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, %size
; CHECK-NEXT:    %ptr1 = inttoptr i32 %1 to i8*
; CHECK-NEXT:    %2 = sub i32 %1, 19
; CHECK-NEXT:    %ptr2 = inttoptr i32 %2 to i8*
; CHECK-NEXT:    ret i8* %ptr2
; CHECK-NEXT:  }

define i8* @test_stacksave() {
  %ptr = call i8* @llvm.stacksave(), !dbg !1
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_stacksave() {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %ptr = inttoptr i32 %frame_top to i8*, !dbg !1
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_stacksave_after_alloca() {
  %ptr1 = alloca i8, i32 11
  %ptr2 = call i8* @llvm.stacksave(), !dbg !1
  ret i8* %ptr2
}

; CHECK-LABEL: define i8* @test_stacksave_after_alloca() {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, 11
; CHECK-NEXT:    %ptr1 = inttoptr i32 %1 to i8*
; CHECK-NEXT:    %ptr2 = inttoptr i32 %1 to i8*, !dbg !1
; CHECK-NEXT:    ret i8* %ptr2
; CHECK-NEXT:  }

define i8* @test_stackrestore(i8* %new_stack) {
  call void @llvm.stackrestore(i8* %new_stack), !dbg !1
  %ptr = alloca i8, i32 5, !dbg !2
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_stackrestore(i8* %new_stack) {
; CHECK-NEXT:    frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = ptrtoint i8* %new_stack to i32, !dbg !1
; CHECK-NEXT:    %2 = sub i32 %1, 5
; CHECK-NEXT:    %ptr = inttoptr i32 %2 to i8*, !dbg !2
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_stackrestore_after_push(i8* %new_stack) {
  %ptr1 = alloca i8, i32 5, !dbg !1
  call void @llvm.stackrestore(i8* %new_stack), !dbg !2
  %ptr2 = alloca i8, i32 6, !dbg !3
  ret i8* %ptr2
}

; CHECK-LABEL: define i8* @test_stackrestore_after_push(i8* %new_stack) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, 5
; CHECK-NEXT:    %ptr1 = inttoptr i32 %1 to i8*, !dbg !1
; CHECK-NEXT:    %2 = ptrtoint i8* %new_stack to i32, !dbg !2
; CHECK-NEXT:    %3 = sub i32 %2, 6
; CHECK-NEXT:    %ptr2 = inttoptr i32 %3 to i8*, !dbg !3
; CHECK-NEXT:    ret i8* %ptr2
; CHECK-NEXT:  }

define i8* @test_stackptr_bb_propagation(i1 %cond) {
  %ptr1 = alloca i8, i32 8
  br i1 %cond, label %IfOne, label %IfNotOne
IfOne:
  ret i8* %ptr1
IfNotOne:
  %ptr2 = alloca i8, i32 4
  ret i8* %ptr2
}

; CHECK-LABEL: define i8* @test_stackptr_bb_propagation(i1 %cond) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %1 = sub i32 %frame_top, 8
; CHECK-NEXT:    %ptr1 = inttoptr i32 %1 to i8*
; CHECK-NEXT:    br i1 %cond, label %IfOne, label %IfNotOne
; CHECK:       IfOne:
; CHECK-NEXT:    %2 = phi i32 [ %1, %0 ]
; CHECK-NEXT:    ret i8* %ptr1
; CHECK:       IfNotOne:
; CHECK-NEXT:    %3 = phi i32 [ %1, %0 ]
; CHECK-NEXT:    %4 = sub i32 %3, 4
; CHECK-NEXT:    %ptr2 = inttoptr i32 %4 to i8*
; CHECK-NEXT:    ret i8* %ptr2
; CHECK-NEXT:  }

define i8* @test_scoped_alloca(i1 %cmp) {
  br label %loop
loop:
  %stacksave = call i8* @llvm.stacksave()
  %ptr = alloca i8, i32 17
  call void @llvm.stackrestore(i8* %stacksave)
  br i1 %cmp, label %done, label %loop
done:
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_scoped_alloca(i1 %cmp) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %1 = phi i32 [ %frame_top, %0 ], [ %3, %loop ]
; CHECK-NEXT:    %stacksave = inttoptr i32 %1 to i8*
; CHECK-NEXT:    %2 = sub i32 %1, 17
; CHECK-NEXT:    %ptr = inttoptr i32 %2 to i8*
; CHECK-NEXT:    %3 = ptrtoint i8* %stacksave to i32
; CHECK-NEXT:    br i1 %cmp, label %done, label %loop
; CHECK:       done:
; CHECK-NEXT:    %4 = phi i32 [ %3, %loop ]
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i8* @test_global_ptr_updates(i32 %size) {
  %ptr0 = call i8* @llvm.stacksave()
  %ptr1 = alloca i8, i32 32
  %ptr2 = alloca i8, i32 %size
  call void @llvm.stackrestore(i8* %ptr0)
  call void @foo()
  ret i8* %ptr2
}

; CHECK-LABEL: define i8* @test_global_ptr_updates(i32 %size) {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr
; CHECK-NEXT:    %ptr0 = inttoptr i32 %frame_top to i8*
; CHECK-NEXT:    %1 = sub i32 %frame_top, 32
; CHECK-NEXT:    store i32 %1, i32* @__sfi_stack_ptr
; CHECK-NEXT:    %ptr1 = inttoptr i32 %1 to i8*
; CHECK-NEXT:    %2 = sub i32 %1, %size
; CHECK-NEXT:    store i32 %2, i32* @__sfi_stack_ptr
; CHECK-NEXT:    %ptr2 = inttoptr i32 %2 to i8*
; CHECK-NEXT:    %3 = ptrtoint i8* %ptr0 to i32
; CHECK-NEXT:    store i32 %3, i32* @__sfi_stack_ptr
; CHECK-NEXT:    call void @foo()
; CHECK-NEXT:    store i32 %frame_top, i32* @__sfi_stack_ptr
; CHECK-NEXT:    ret i8* %ptr2
; CHECK-NEXT:  }

define i32 @_start_minsfi(i32 %args) {
  ret i32 0
}

; CHECK-LABEL: define i32 @_start_minsfi(i32 %args) {
; CHECK-NEXT:    store i32 %args, i32* @__sfi_stack_ptr
; CHECK-NEXT:    ret i32 0
; CHECK-NEXT:  }

!1 = metadata !{i32 138, i32 0, metadata !1, null}
!2 = metadata !{i32 142, i32 0, metadata !2, null}
!3 = metadata !{i32 144, i32 0, metadata !3, null}
