; Test handling of constants in function blocks.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

define void @TestIntegers() {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 2>              |      i8:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c0 = i8 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 10>             |        %c1 = i8 5;
; CHECK-NEXT:    {{.*}}|      3: <4, 11>             |        %c2 = i8 -5;
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c3 = i8 undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 4>              |      i16:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c4 = i16 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 20>             |        %c5 = i16 10;
; CHECK-NEXT:    {{.*}}|      3: <4, 21>             |        %c6 = i16 -10;
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c7 = i16 undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 5>              |      i32:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c8 = i32 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 40>             |        %c9 = i32 20;
; CHECK-NEXT:    {{.*}}|      3: <4, 41>             |        %c10 = i32 -20;
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c11 = i32 undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 6>              |      i64:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c12 = i64 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 60>             |        %c13 = i64 30;
; CHECK-NEXT:    {{.*}}|      3: <4, 61>             |        %c14 = i64 -30;
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c15 = i64 undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 7>              |      i1:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c16 = i1 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 3>              |        %c17 = i1 1;
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c18 = i1 undef;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:

  ; Test various sized integers
  %v0 = or i1 true, false
  %v1 = add i8 0, 0
  %v2 = add i8 5, 0
  %v3 = add i8 -5, 0
  %v4 = and i16 10, 0
  %v5 = add i16 -10, 0
  %v6 = add i32 20, 0
  %v7 = add i32 -20, 0
  %v8 = add i64 30, 0
  %v9 = add i64 -30, 0

; CHECK-NEXT:    {{.*}}|    3: <2, 2, 3, 11>         |    %v0 = or i1 %c17, %c16;
; CHECK-NEXT:    {{.*}}|    3: <2, 20, 20, 0>        |    %v1 = add i8 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 20, 21, 0>        |    %v2 = add i8 %c1, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 20, 22, 0>        |    %v3 = add i8 %c2, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 18, 19, 10>       |    %v4 = and i16 %c5, %c4;
; CHECK-NEXT:    {{.*}}|    3: <2, 18, 20, 0>        |    %v5 = add i16 %c6, %c4;
; CHECK-NEXT:    {{.*}}|    3: <2, 16, 17, 0>        |    %v6 = add i32 %c9, %c8;
; CHECK-NEXT:    {{.*}}|    3: <2, 16, 18, 0>        |    %v7 = add i32 %c10, %c8;
; CHECK-NEXT:    {{.*}}|    3: <2, 14, 15, 0>        |    %v8 = add i64 %c13, %c12;
; CHECK-NEXT:    {{.*}}|    3: <2, 14, 16, 0>        |    %v9 = add i64 %c14, %c12;

  ; Test undefined integer values.
  %v10 = xor i1 undef, 0
  %v11 = add i8 undef, 0
  %v12 = add i16 undef, 0
  %v13 = add i32 undef, 0
  %v14 = add i64 undef, 0
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 11, 13, 12>       |    %v10 = xor i1 %c18, %c16;
; CHECK-NEXT:    {{.*}}|    3: <2, 27, 30, 0>        |    %v11 = add i8 %c3, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 24, 27, 0>        |    %v12 = add i16 %c7, %c4;
; CHECK-NEXT:    {{.*}}|    3: <2, 21, 24, 0>        |    %v13 = add i32 %c11, %c8;
; CHECK-NEXT:    {{.*}}|    3: <2, 18, 21, 0>        |    %v14 = add i64 %c15, %c12;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;
}

define void @TestFloats() {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 0>              |        %c0 = float 0;
; CHECK-NEXT:    {{.*}}|      3: <6, 1065353216>     |        %c1 = float 1;
; CHECK-NEXT:    {{.*}}|      3: <6, 1088421888>     |        %c2 = float 7;
; CHECK-NEXT:    {{.*}}|      3: <6, 1090519040>     |        %c3 = float 8;
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c4 = float undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      double:
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c5 = double 6;
; CHECK-NEXT:          |        4618441417868443648> |
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c6 = double 1;
; CHECK-NEXT:          |        4607182418800017408> |
; CHECK-NEXT:    {{.*}}|      3: <6, 0>              |        %c7 = double 0;
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c8 = double 5;
; CHECK-NEXT:          |        4617315517961601024> |
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c9 = double undef;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:

  ; Test float and double constants
  %1 = fadd float 1.0, 0.0
  %2 = fadd double 1.0, 0.0
  %3 = fsub float 7.000000e+00, 8.000000e+00
  %4 = fsub double 5.000000e+00, 6.000000e+00

; CHECK-NEXT:    {{.*}}|    3: <2, 9, 10, 0>         |    %v0 = fadd float %c1, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 4, 0>          |    %v1 = fadd double %c6, %c7;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 9, 1>         |    %v2 = fsub float %c2, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 8, 1>          |    %v3 = fsub double %c8, %c5;

  ; Test undefined float and double.
  %5 = fadd float undef, 0.0
  %6 = fsub double undef, 6.000000e+00
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 10, 14, 0>        |    %v4 = fadd float %c4, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 6, 10, 1>         |    %v5 = fsub double %c9, %c5;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

; Test float Nan, +Inf, -Inf.
; Note: llvm-as doesn't accept float hex values. Only accepts double
; hex values.
define float @GetFloatNan() {
  ; Generated from NAN in <math.h>
  ret float 0x7FF8000000000000

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 2143289344>     |        %c0 = float nan;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret float %c0;

}

define float @GetFloatInf() {
  ; Generated from INFINITY in <math.h>
  ret float 0x7FF0000000000000

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 2139095040>     |        %c0 = float inf;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret float %c0;

}

define float @GetFloatNegInf() {
  ; Generated from -INFINITY in <math.h>
  ret float 0xFFF0000000000000

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 4286578688>     |        %c0 = float -inf;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret float %c0;
}

; Test double Nan, +Inf, -Inf.
define double @GetDoubleNan() {
  ; Generated from NAN in <math.h>
  ret double 0x7FF8000000000000

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      double:
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c0 = double nan;
; CHECK-NEXT:          |        9221120237041090560> |
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret double %c0;

}

define double @GetDoubleInf() {
  ; Generated from INFINITY in <math.h>
  ret double 0x7FF0000000000000

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      double:
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c0 = double inf;
; CHECK-NEXT:          |        9218868437227405312> |
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret double %c0;

}

define double @GetDoubleNegInf() {
  ; Generated from -INFINITY in <math.h>
  ret double 0xFFF0000000000000

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      double:
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c0 = double -inf;
; CHECK-NEXT:          |        18442240474082181120>|
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <10, 1>               |    ret double %c0;

}

