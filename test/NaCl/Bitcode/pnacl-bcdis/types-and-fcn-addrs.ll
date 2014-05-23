; Simple test to see if we handle all types. Note: Types are generated
; via function declarations.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis | FileCheck %s

declare void @func();

declare i1 @test(i32, float, i32, i8, i16);

declare i64 @merge(i32, i32);

declare double @Float2Double(float);

declare float @Double2Float(double);

declare void @indefargs(i32, ...);

declare <4 x i32> @Mul(<4 x i32>, <4 x i32>);

; Show example where function name can't be modeled as a char6.
declare i32 @foo$bar(i32);

; CHECK:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:          | 0>                          |
; CHECK-NEXT:      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; CHECK-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; CHECK-NEXT:      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; CHECK-NEXT:      36:0|    <1, 14>                  |
; CHECK-NEXT:      38:4|    2: <65533, 14, 4, 0, 1,  |
; CHECK-NEXT:          |        3, 0, 2, 8, 0, 3, 0, |
; CHECK-NEXT:          |        1, 8>                |
; CHECK-NEXT:      43:2|    2: <65533, 4, 1, 1, 0, 2,|
; CHECK-NEXT:          |        8, 0, 3, 0, 1, 7>    |
; CHECK-NEXT:      48:0|    2: <65533, 4, 1, 1, 0, 2,|
; CHECK-NEXT:          |        8, 0, 3, 0, 4>       |
; CHECK-NEXT:      52:1|    2: <65533, 4, 1, 2, 0, 2,|
; CHECK-NEXT:          |        8, 0, 3, 0, 4>       |
; CHECK-NEXT:      56:2|    <1, 11>                  |
; CHECK-NEXT:      58:6|    2: <65533, 11, 2, 1, 1,  |
; CHECK-NEXT:          |        0, 1, 5>             |
; CHECK-NEXT:      61:7|    2: <65533, 2, 1, 4, 0, 2,|
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:      65:0|    2: <65533, 2, 1, 4, 1, 0>|
; CHECK-NEXT:      68:1|    2: <65533, 2, 1, 6, 0, 2,|
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:      71:2|    <1, 12>                  |
; CHECK-NEXT:      73:6|    2: <65533, 12, 4, 1, 20, |
; CHECK-NEXT:          |        0, 2, 6, 0, 2, 4, 0, |
; CHECK-NEXT:          |        2, 4>                |
; CHECK-NEXT:      79:1|    2: <65533, 4, 1, 2, 0, 2,|
; CHECK-NEXT:          |        6, 0, 2, 6, 0, 1, 4> |
; CHECK-NEXT:      84:4|    2: <65533, 4, 1, 3, 0, 2,|
; CHECK-NEXT:          |        6, 0, 1, 5, 0, 1, 4> |
; CHECK-NEXT:      89:7|    2: <65533, 1, 1, 10>     |
; CHECK-NEXT:      91:7|    2: <65533, 2, 1, 10, 0,  |
; CHECK-NEXT:          |        2, 6>                |
; CHECK-NEXT:      95:0|    2: <65533, 1, 1, 15>     |
; CHECK-NEXT:      97:0|    2: <65533, 3, 1, 43, 0,  |
; CHECK-NEXT:          |        2, 6, 0, 1, 5>       |
; CHECK-NEXT:     101:2|    2: <65533, 4, 1, 24, 0,  |
; CHECK-NEXT:          |        2, 6, 0, 2, 6, 0, 2, |
; CHECK-NEXT:          |        4>                   |
; CHECK-NEXT:     106:5|    <1, 19>                  |
; CHECK-NEXT:     109:1|    2: <65533, 19, 3, 1, 0,  |
; CHECK-NEXT:          |        0, 2, 6, 0, 1, 1>    |
; CHECK-NEXT:     113:3|    2: <65533, 2, 1, 1, 0, 2,|
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:     116:4|    2: <65533, 2, 1, 2, 0, 2,|
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:     119:5|    2: <65533, 3, 1, 3, 0, 3,|
; CHECK-NEXT:          |        0, 1, 8>             |
; CHECK-NEXT:     123:2|    2: <65533, 2, 1, 4, 0, 2,|
; CHECK-NEXT:          |        6>                   |
; CHECK-NEXT:     126:3|    2: <65533, 3, 1, 4, 0, 2,|
; CHECK-NEXT:          |        6, 0, 2, 6>          |
; CHECK-NEXT:     130:5|  0: <65534>                 |  }
; CHECK-NEXT:     132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; CHECK-NEXT:     140:0|    2: <65533, 4, 1, 21, 0,  |
; CHECK-NEXT:          |        1, 1, 0, 3, 0, 1, 5> |
; CHECK-NEXT:     144:7|    3: <1, 17>               |    count 17;
; CHECK-NEXT:     147:4|    3: <7, 32>               |    @t0 = i32;
; CHECK-NEXT:     150:7|    3: <3>                   |    @t1 = float;
; CHECK-NEXT:     152:6|    3: <12, 4, 0>            |    @t2 = <4 x i32>;
; CHECK-NEXT:     156:1|    3: <4>                   |    @t3 = double;
; CHECK-NEXT:     158:0|    3: <2>                   |    @t4 = void;
; CHECK-NEXT:     159:7|    3: <7, 8>                |    @t5 = i8;
; CHECK-NEXT:     162:4|    3: <7, 16>               |    @t6 = i16;
; CHECK-NEXT:     165:1|    4: <21, 0, 4>            |    @t7 = void ();
; CHECK-NEXT:     167:0|    3: <7, 1>                |    @t8 = i1;
; CHECK-NEXT:     169:5|    4: <21, 0, 8, 0, 1, 0, 5,|    @t9 = 
; CHECK-NEXT:          |        6>                   |        i1 (i32, float, i32, i8, i16);
; CHECK-NEXT:     174:5|    3: <7, 64>               |    @t10 = i64;
; CHECK-NEXT:     178:0|    4: <21, 0, 10, 0, 0>     |    @t11 = i64 (i32, i32);
; CHECK-NEXT:     181:1|    4: <21, 0, 3, 1>         |    @t12 = double (float);
; CHECK-NEXT:     183:5|    4: <21, 0, 1, 3>         |    @t13 = float (double);
; CHECK-NEXT:     186:1|    4: <21, 0, 4, 0>         |    @t14 = void (i32);
; CHECK-NEXT:     188:5|    4: <21, 0, 2, 2, 2>      |    @t15 = 
; CHECK-NEXT:          |                             |        <4 x i32> 
; CHECK-NEXT:          |                             |        (<4 x i32>, <4 x i32>);
; CHECK-NEXT:     191:6|    4: <21, 0, 0, 0>         |    @t16 = i32 (i32);
; CHECK-NEXT:     194:2|  0: <65534>                 |  }
; CHECK-NEXT:     196:0|  3: <8, 7, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT:     200:6|  3: <8, 9, 0, 1, 0>         |  declare external 
; CHECK-NEXT:          |                             |      i1 @f1(i32, float, i32, i8, i16);
; CHECK-NEXT:     205:4|  3: <8, 11, 0, 1, 0>        |  declare external i64 @f2(i32, i32);
; CHECK-NEXT:     210:2|  3: <8, 12, 0, 1, 0>        |  declare external double @f3(float);
; CHECK-NEXT:     215:0|  3: <8, 13, 0, 1, 0>        |  declare external float @f4(double);
; CHECK-NEXT:     219:6|  3: <8, 14, 0, 1, 0>        |  declare external void @f5(i32);
; CHECK-NEXT:     224:4|  3: <8, 15, 0, 1, 0>        |  declare external 
; CHECK-NEXT:          |                             |      <4 x i32> 
; CHECK-NEXT:          |                             |      @f6(<4 x i32>, <4 x i32>);
; CHECK-NEXT:     229:2|  3: <8, 16, 0, 1, 0>        |  declare external i32 @f7(i32);
; CHECK-NEXT:     234:0|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     240:0|    3: <5, 0>                |    count 0;
; CHECK-NEXT:     242:6|  0: <65534>                 |  }
; CHECK-NEXT:     244:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     252:0|    6: <1, 1, 116, 101, 115, |    @f1 : "test";
; CHECK-NEXT:          |        116>                 |
; CHECK-NEXT:     257:1|    6: <1, 2, 109, 101, 114, |    @f2 : "merge";
; CHECK-NEXT:          |        103, 101>            |
; CHECK-NEXT:     263:0|    6: <1, 3, 70, 108, 111,  |    @f3 : "Float2Double";
; CHECK-NEXT:          |        97, 116, 50, 68, 111,|
; CHECK-NEXT:          |        117, 98, 108, 101>   |
; CHECK-NEXT:     274:1|    6: <1, 4, 68, 111, 117,  |    @f4 : "Double2Float";
; CHECK-NEXT:          |        98, 108, 101, 50, 70,|
; CHECK-NEXT:          |        108, 111, 97, 116>   |
; CHECK-NEXT:     285:2|    6: <1, 5, 105, 110, 100, |    @f5 : "indefargs";
; CHECK-NEXT:          |        101, 102, 97, 114,   |
; CHECK-NEXT:          |        103, 115>            |
; CHECK-NEXT:     294:1|    6: <1, 0, 102, 117, 110, |    @f0 : "func";
; CHECK-NEXT:          |        99>                  |
; CHECK-NEXT:     299:2|    5: <1, 7, 102, 111, 111, |    @f7 : {'f', 'o', 'o',  36, 'b', 
; CHECK-NEXT:          |        36, 98, 97, 114>     |           'a', 'r'}
; CHECK-NEXT:     307:4|    6: <1, 6, 77, 117, 108>  |    @f6 : "Mul";
; CHECK-NEXT:     311:7|  0: <65534>                 |  }
; CHECK-NEXT:     316:0|0: <65534>                   |}
