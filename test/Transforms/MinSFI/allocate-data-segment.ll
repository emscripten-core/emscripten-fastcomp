; RUN: opt %s -minsfi-allocate-data-segment -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

; Every global variable in PNaCl IR must have an initializer. This is an
; exhaustive list of all the possible formats.

@a = global [1 x i8] zeroinitializer                       ; no alignment
@b = global [3 x i8] c"ABC"                                ; no alignment
@c = global [5 x i8] c"ABCDE", align 16                    ; explicit 16B align
@d = global i32 ptrtoint ([3 x i8]* @b to i32)             ; implicit 4B align
@e = global i32 add (i32 ptrtoint ([5 x i8]* @c to i32), i32 3), align 2
                                                           ; implicit 4B align
@f = global <{ [1 x i8], i32 }> 
       <{ [1 x i8] zeroinitializer, 
          i32 ptrtoint ([5 x i8]* @c to i32) }>            ; no alignment
@g = global [1 x i8] zeroinitializer                       ; no alignment

; Use each variable to verify its location
 
define void @use_variables() {
  %a = load [1 x i8]* @a
  %b = load [3 x i8]* @b
  %c = load [5 x i8]* @c
  %d = load i32* @d
  %e = load i32* @e
  %f = load <{ [1 x i8], i32 }>* @f
  %g = load [1 x i8]* @g
  ret void
}

; CHECK: %__sfi_data_segment = type <{ [1 x i8], [3 x i8], [12 x i8], [5 x i8], [3 x i8], i32, i32, <{ [1 x i8], i32 }>, [1 x i8] }>
; CHECK: @__sfi_data_segment = constant %__sfi_data_segment <{ [1 x i8] zeroinitializer, [3 x i8] c"ABC", [12 x i8] zeroinitializer, [5 x i8] c"ABCDE", [3 x i8] zeroinitializer, i32 65537, i32 65555, <{ [1 x i8], i32 }> <{ [1 x i8] zeroinitializer, i32 65552 }>, [1 x i8] zeroinitializer }>
; CHECK: @__sfi_data_segment_size = constant i32 38 

; CHECK-LABEL: define void @use_variables() {
; CHECK-NEXT:    %a = load [1 x i8]* inttoptr (i32 65536 to [1 x i8]*)
; CHECK-NEXT:    %b = load [3 x i8]* inttoptr (i32 65537 to [3 x i8]*)
; CHECK-NEXT:    %c = load [5 x i8]* inttoptr (i32 65552 to [5 x i8]*)
; CHECK-NEXT:    %d = load i32* inttoptr (i32 65560 to i32*)
; CHECK-NEXT:    %e = load i32* inttoptr (i32 65564 to i32*)
; CHECK-NEXT:    %f = load <{ [1 x i8], i32 }>* inttoptr (i32 65568 to <{ [1 x i8], i32 }>*)
; CHECK-NEXT:    %g = load [1 x i8]* inttoptr (i32 65573 to [1 x i8]*)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }
