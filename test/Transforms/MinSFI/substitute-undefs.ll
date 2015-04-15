; RUN: opt %s -minsfi-substitute-undefs -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define i1 @test_undef_i1() {
  ret i1 undef                             ; replaced with 0x0
}

; CHECK-LABEL: define i1 @test_undef_i1() {
; CHECK-NEXT:    ret i1 false
; CHECK-NEXT:  }

define i8 @test_undef_i8() {
  ret i8 undef                             ; replaced with 0xBE
}

; CHECK-LABEL: define i8 @test_undef_i8() {
; CHECK-NEXT:    ret i8 -66
; CHECK-NEXT:  }

define i64 @test_undef_i64() {
  ret i64 undef                            ; replaced with 0xBAADF00DCAFEBABE
}

; CHECK-LABEL: define i64 @test_undef_i64() {
; CHECK-NEXT:    ret i64 -4995072469653079362
; CHECK-NEXT:  }

define <8 x i16> @test_undef_i16vec() {
  ret <8 x i16> undef                      ; replace with a vector of 0xBABE
}

; CHECK-LABEL: define <8 x i16> @test_undef_i16vec() {
; CHECK-NEXT:    ret <8 x i16> <i16 -17730, i16 -17730, i16 -17730, i16 -17730, i16 -17730, i16 -17730, i16 -17730, i16 -17730>
; CHECK-NEXT:  }

define float @test_undef_float() {
  ret float undef                          ; replaced with pi
}

; CHECK-LABEL: define float @test_undef_float() {
; CHECK-NEXT:    ret float 0x400921FB60000000
; CHECK-NEXT:  }

define double @test_undef_double() {
  ret double undef                         ; replaced with pi
}

; CHECK-LABEL: define double @test_undef_double() {
; CHECK-NEXT:    ret double 0x400921FB54442EEA
; CHECK-NEXT:  }

define <4 x float> @test_undef_floatvec() {
  ret <4 x float> undef                    ; replaced with a vector of pi
}

; CHECK-LABEL: define <4 x float> @test_undef_floatvec() {
; CHECK-NEXT:    ret <4 x float> <float 0x400921FB60000000, float 0x400921FB60000000, float 0x400921FB60000000, float 0x400921FB60000000>
; CHECK-NEXT:  }



declare void @foo(i32, float)

define void @test_more_operands() {
  call void @foo(i32 undef, float undef)
  ret void
}

; CHECK-LABEL: define void @test_more_operands() {
; CHECK-NEXT:    call void @foo(i32 -889275714, float 0x400921FB60000000)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define void @test_more_basicblocks() {
entry:
  br label %loop

loop:
  %i = phi i16 [ undef, %entry ], [ %next, %loop ]
  %next = add i16 %i, 1
  br i1 undef, label %exit, label %loop

exit:
  ret void
}

; CHECK-LABEL: define void @test_more_basicblocks() {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %loop
; CHECK:       loop:
; CHECK-NEXT:    %i = phi i16 [ -17730, %entry ], [ %next, %loop ]
; CHECK-NEXT:    %next = add i16 %i, 1
; CHECK-NEXT:    br i1 false, label %exit
; CHECK:       exit:
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }
