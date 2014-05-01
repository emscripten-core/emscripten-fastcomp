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

; CHECK:            0:0 <65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:            8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:            0>                          |
; CHECK-NEXT:      16:0 <65535, 8, 2>                |module {  // BlockID = 8
; CHECK-NEXT:      24:0   <1, 1>                     |  version 1;
; CHECK-NEXT:      26:4   <65535, 0, 2>              |  abbreviations {  // BlockID = 0
; CHECK-NEXT:      27:6   <65534>                    |  }
; CHECK-NEXT:     132:0   <65535, 17, 3>             |  types {  // BlockID = 17
; CHECK-NEXT:     149:2     <1, 16>                  |    count 16;
; CHECK-NEXT:     151:7     <7, 32>                  |    @t0 = i32;
; CHECK-NEXT:     155:2     <3>                      |    @t1 = float;
; CHECK-NEXT:     157:1     <12, 4, 0>               |    @t2 = <4 x i32>;
; CHECK-NEXT:     160:4     <4>                      |    @t3 = double;
; CHECK-NEXT:     162:3     <2>                      |    @t4 = void;
; CHECK-NEXT:     164:2     <7, 8>                   |    @t5 = i8;
; CHECK-NEXT:     166:7     <7, 16>                  |    @t6 = i16;
; CHECK-NEXT:     169:4     <21, 0, 4>               |    @t7 = void ();
; CHECK-NEXT:     171:3     <7, 1>                   |    @t8 = i1;
; CHECK-NEXT:     174:0     <21, 0, 8, 0, 1, 0, 5, 6>|    @t9 = 
; CHECK-NEXT:                                        |        i1 (i32, float, i32, i8, i16);
; CHECK-NEXT:     179:0     <7, 64>                  |    @t10 = i64;
; CHECK-NEXT:     182:3     <21, 0, 10, 0, 0>        |    @t11 = i64 (i32, i32);
; CHECK-NEXT:     185:4     <21, 0, 3, 1>            |    @t12 = double (float);
; CHECK-NEXT:     188:0     <21, 0, 1, 3>            |    @t13 = float (double);
; CHECK-NEXT:     190:4     <21, 0, 4, 0>            |    @t14 = void (i32);
; CHECK-NEXT:     193:0     <21, 0, 2, 2, 2>         |    @t15 = 
; CHECK-NEXT:                                        |        <4 x i32> 
; CHECK-NEXT:                                        |        (<4 x i32>, <4 x i32>);
; CHECK-NEXT:     196:1   <65534>                    |  }
; CHECK-NEXT:     200:0   <8, 7, 0, 1, 0>            |  declare external void @f0();
; CHECK-NEXT:     204:6   <8, 9, 0, 1, 0>            |  declare external 
; CHECK-NEXT:                                        |      i1 @f1(i32, float, i32, i8, i16);
; CHECK-NEXT:     209:4   <8, 11, 0, 1, 0>           |  declare external i64 @f2(i32, i32);
; CHECK-NEXT:     214:2   <8, 12, 0, 1, 0>           |  declare external double @f3(float);
; CHECK-NEXT:     219:0   <8, 13, 0, 1, 0>           |  declare external float @f4(double);
; CHECK-NEXT:     223:6   <8, 14, 0, 1, 0>           |  declare external void @f5(i32);
; CHECK-NEXT:     228:4   <8, 15, 0, 1, 0>           |  declare external 
; CHECK-NEXT:                                        |      <4 x i32> 
; CHECK-NEXT:                                        |      @f6(<4 x i32>, <4 x i32>);
; CHECK-NEXT:     233:2   <65535, 19, 4>             |  globals {  // BlockID = 19
; CHECK-NEXT:     240:0     <5, 0>                   |
; CHECK-NEXT:     242:6   <65534>                    |  }
; CHECK-NEXT:     244:0   <65535, 14, 3>             |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     252:0     <1, 1, 116, 101, 115, 116|
; CHECK-NEXT:                >                       |
; CHECK-NEXT:     257:1     <1, 2, 109, 101, 114,    |
; CHECK-NEXT:                103, 101>               |
; CHECK-NEXT:     263:0     <1, 3, 70, 108, 111, 97, |
; CHECK-NEXT:                116, 50, 68, 111, 117,  |
; CHECK-NEXT:                98, 108, 101>           |
; CHECK-NEXT:     274:1     <1, 4, 68, 111, 117, 98, |
; CHECK-NEXT:                108, 101, 50, 70, 108,  |
; CHECK-NEXT:                111, 97, 116>           |
; CHECK-NEXT:     285:2     <1, 5, 105, 110, 100,    |
; CHECK-NEXT:                101, 102, 97, 114, 103, |
; CHECK-NEXT:                115>                    |
; CHECK-NEXT:     294:1     <1, 0, 102, 117, 110, 99>|
; CHECK-NEXT:     299:2     <1, 6, 77, 117, 108>     |
; CHECK-NEXT:     303:5   <65534>                    |  }
; CHECK-NEXT:     304:0 <65534>                      |}
