; Test return instructions for various types.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

define internal void @fvoid() {
  ret void

; CHECK:               |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

define internal i32 @fi32(i32 %p0) {
  ret i32 %p0

; CHECK:               |                             |  %b0:
; CHECK:         {{.*}}|    3: <10, 1>               |    ret i32 %p0;

}


define internal i64 @fi64(i64 %p0) {
  ret i64 %p0

; CHECK:               |                             |  %b0:
; CHECK:         {{.*}}|    3: <10, 1>               |    ret i64 %p0;

}

define internal float @ffloat(float %p0) {
  ret float %p0

; CHECK:               |                             |  %b0:
; CHECK:         {{.*}}|    3: <10, 1>               |    ret float %p0;

}


define internal double @fdouble(double %p0) {
  ret double %p0

; CHECK:               |                             |  %b0:
; CHECK:         {{.*}}|    3: <10, 1>               |    ret double %p0;

}

define internal <4 x i32> @fi32vec(<4 x i32> %p0) {
  ret <4 x i32> %p0

; CHECK:               |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret <4 x i32> %p0;

}

