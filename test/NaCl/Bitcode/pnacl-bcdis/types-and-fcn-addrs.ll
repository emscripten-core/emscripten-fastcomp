; Simple test to see if we handle all types. Note: Types are generated
; via function declarations.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

declare void @func();

declare i32 @test(i32, float, i32, i32, i64);

declare i64 @merge(i32, i32);

declare double @Float2Double(float);

declare float @Double2Float(double);

declare void @indefargs(i32, ...);

declare <4 x i32> @Mul(<4 x i32>, <4 x i32>);

; Show example where function name can't be modeled as a char6.
declare i32 @foo$bar(i32);

; CHECK:         0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:       | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:       | 0>                          |
; CHECK-NEXT:   16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; CHECK-NEXT:   24:0|  3: <1, 1>                  |  version 1;
; CHECK-NEXT:   26:4|  1: <65535, 17, 2>          |  types {  // BlockID = 17
; CHECK-NEXT:   36:0|    3: <1, 14>               |    count 14;
; CHECK-NEXT:   38:4|    3: <7, 32>               |    @t0 = i32;
; CHECK-NEXT:   41:6|    3: <3>                   |    @t1 = float;
; CHECK-NEXT:   43:4|    3: <12, 4, 0>            |    @t2 = <4 x i32>;
; CHECK-NEXT:   46:6|    3: <7, 64>               |    @t3 = i64;
; CHECK-NEXT:   50:0|    3: <4>                   |    @t4 = double;
; CHECK-NEXT:   51:6|    3: <2>                   |    @t5 = void;
; CHECK-NEXT:   53:4|    3: <21, 0, 5>            |    @t6 = void ();
; CHECK-NEXT:   56:6|    3: <21, 0, 0, 0, 1, 0, 0,|    @t7 = 
; CHECK-NEXT:       |        3>                   |        i32 (i32, float, i32, i32, i64)
; CHECK-NEXT:       |                             |        ;
; CHECK-NEXT:   63:6|    3: <21, 0, 3, 0, 0>      |    @t8 = i64 (i32, i32);
; CHECK-NEXT:   68:4|    3: <21, 0, 4, 1>         |    @t9 = double (float);
; CHECK-NEXT:   72:4|    3: <21, 0, 1, 4>         |    @t10 = float (double);
; CHECK-NEXT:   76:4|    3: <21, 0, 5, 0>         |    @t11 = void (i32);
; CHECK-NEXT:   80:4|    3: <21, 0, 2, 2, 2>      |    @t12 = 
; CHECK-NEXT:       |                             |        <4 x i32> 
; CHECK-NEXT:       |                             |        (<4 x i32>, <4 x i32>);
; CHECK-NEXT:   85:2|    3: <21, 0, 0, 0>         |    @t13 = i32 (i32);
; CHECK-NEXT:   89:2|  0: <65534>                 |  }
; CHECK-NEXT:   92:0|  3: <8, 6, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT:   96:6|  3: <8, 7, 0, 1, 0>         |  declare external 
; CHECK-NEXT:       |                             |      i32 
; CHECK-NEXT:       |                             |      @f1(i32, float, i32, i32, i64);
; CHECK-NEXT:  101:4|  3: <8, 8, 0, 1, 0>         |  declare external i64 @f2(i32, i32);
; CHECK-NEXT:  106:2|  3: <8, 9, 0, 1, 0>         |  declare external double @f3(float);
; CHECK-NEXT:  111:0|  3: <8, 10, 0, 1, 0>        |  declare external float @f4(double);
; CHECK-NEXT:  115:6|  3: <8, 11, 0, 1, 0>        |  declare external void @f5(i32);
; CHECK-NEXT:  120:4|  3: <8, 12, 0, 1, 0>        |  declare external 
; CHECK-NEXT:       |                             |      <4 x i32> 
; CHECK-NEXT:       |                             |      @f6(<4 x i32>, <4 x i32>);
; CHECK-NEXT:  125:2|  3: <8, 13, 0, 1, 0>        |  declare external i32 @f7(i32);
; CHECK-NEXT:  130:0|  1: <65535, 19, 2>          |  globals {  // BlockID = 19
; CHECK-NEXT:  136:0|    3: <5, 0>                |    count 0;
; CHECK-NEXT:  138:4|  0: <65534>                 |  }
; CHECK-NEXT:  140:0|  1: <65535, 14, 2>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:  148:0|    3: <1, 1, 116, 101, 115, |    @f1 : "test";
; CHECK-NEXT:       |        116>                 |
; CHECK-NEXT:  156:4|    3: <1, 2, 109, 101, 114, |    @f2 : "merge";
; CHECK-NEXT:       |        103, 101>            |
; CHECK-NEXT:  166:4|    3: <1, 3, 70, 108, 111,  |    @f3 : "Float2Double";
; CHECK-NEXT:       |        97, 116, 50, 68, 111,|
; CHECK-NEXT:       |        117, 98, 108, 101>   |
; CHECK-NEXT:  187:0|    3: <1, 4, 68, 111, 117,  |    @f4 : "Double2Float";
; CHECK-NEXT:       |        98, 108, 101, 50, 70,|
; CHECK-NEXT:       |        108, 111, 97, 116>   |
; CHECK-NEXT:  207:4|    3: <1, 5, 105, 110, 100, |    @f5 : "indefargs";
; CHECK-NEXT:       |        101, 102, 97, 114,   |
; CHECK-NEXT:       |        103, 115>            |
; CHECK-NEXT:  223:4|    3: <1, 0, 102, 117, 110, |    @f0 : "func";
; CHECK-NEXT:       |        99>                  |
; CHECK-NEXT:  232:0|    3: <1, 7, 102, 111, 111, |    @f7 : {'f', 'o', 'o',  36, 'b', 
; CHECK-NEXT:       |        36, 98, 97, 114>     |           'a', 'r'}
; CHECK-NEXT:  245:0|    3: <1, 6, 77, 117, 108>  |    @f6 : "Mul";
; CHECK-NEXT:  252:0|  0: <65534>                 |  }
; CHECK-NEXT:  256:0|0: <65534>                   |}
