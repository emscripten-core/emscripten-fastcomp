; Test handling of constants in function blocks.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis | FileCheck %s

define void @TestIntegers() {
  ; Test various sized integers
  %1 = add i1 true, false
  %2 = add i8 0, 0
  %3 = add i8 5, 0
  %4 = add i8 -5, 0
  %5 = add i16 10, 0
  %6 = add i16 -10, 0
  %7 = add i32 20, 0
  %8 = add i32 -20, 0
  %9 = add i64 30, 0
  %10 = add i64 -30, 0
  ; Test undefined integer values.
  %11 = add i1 undef, 0
  %12 = add i8 undef, 0
  %13 = add i16 undef, 0
  %14 = add i32 undef, 0
  %15 = add i64 undef, 0
  ret void
}

define void @TestFloats() {
  ; Test float and double constants
  %1 = fadd float 1.0, 0.0
  %2 = fadd double 1.0, 0.0
  %3 = fsub float 7.000000e+00, 8.000000e+00
  %4 = fsub double 5.000000e+00, 6.000000e+00
  ; Test undefined float and double.
  %5 = fadd float undef, 0.0
  %6 = fsub double undef, 6.000000e+00
  ret void
}

; Test float Nan, +Inf, -Inf.
; Note: llvm-as doesn't accept float hex values. Only accepts double
; hex values.
define float @GetFloatNan() #0 {
entry:
  ; Generated from NAN in <math.h>
  ret float 0x7FF8000000000000
}

define float @GetFloatInf() #0 {
entry:
  ; Generated from INFINITY in <math.h>
  ret float 0x7FF0000000000000
}

define float @GetFloatNegInf() #0 {
entry:
  ; Generated from -INFINITY in <math.h>
  ret float 0xFFF0000000000000
}

; Test double Nan, +Inf, -Inf.
define double @GetDoubleNan() #0 {
entry:
  ; Generated from NAN in <math.h>
  ret double 0x7FF8000000000000
}

define double @GetDoubleInf() #0 {
entry:
  ; Generated from INFINITY in <math.h>
  ret double 0x7FF0000000000000
}

define double @GetDoubleNegInf() #0 {
entry:
  ; Generated from -INFINITY in <math.h>
  ret double 0xFFF0000000000000
}

; CHECK:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:          | 0>                          |
; CHECK-NEXT:      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; CHECK-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; CHECK-NEXT:      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; CHECK-NEXT:      36:0|    1: <1, 14>               |    valuesymtab:
; CHECK-NEXT:      38:4|    2: <65533, 14, 4, 0, 1,  |      @a0 = abbrev <fixed(3), vbr(8), 
; CHECK-NEXT:          |        3, 0, 2, 8, 0, 3, 0, |                   array(fixed(8))>;
; CHECK-NEXT:          |        1, 8>                |
; CHECK-NEXT:      43:2|    2: <65533, 4, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8), 
; CHECK-NEXT:          |        8, 0, 3, 0, 1, 7>    |                   array(fixed(7))>;
; CHECK-NEXT:      48:0|    2: <65533, 4, 1, 1, 0, 2,|      @a2 = abbrev <1, vbr(8), 
; CHECK-NEXT:          |        8, 0, 3, 0, 4>       |                   array(char6)>;
; CHECK-NEXT:      52:1|    2: <65533, 4, 1, 2, 0, 2,|      @a3 = abbrev <2, vbr(8), 
; CHECK-NEXT:          |        8, 0, 3, 0, 4>       |                   array(char6)>;
; CHECK-NEXT:      56:2|    1: <1, 11>               |    constants:
; CHECK-NEXT:      58:6|    2: <65533, 11, 2, 1, 1,  |      @a0 = abbrev <1, fixed(4)>;
; CHECK-NEXT:          |        0, 1, 4>             |
; CHECK-NEXT:      61:7|    2: <65533, 2, 1, 4, 0, 2,|      @a1 = abbrev <4, vbr(8)>;
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:      65:0|    2: <65533, 2, 1, 4, 1, 0>|      @a2 = abbrev <4, 0>;
; CHECK-NEXT:      68:1|    2: <65533, 2, 1, 6, 0, 2,|      @a3 = abbrev <6, vbr(8)>;
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:      71:2|    1: <1, 12>               |    function:
; CHECK-NEXT:      73:6|    2: <65533, 12, 4, 1, 20, |      @a0 = abbrev <20, vbr(6), vbr(4),
; CHECK-NEXT:          |        0, 2, 6, 0, 2, 4, 0, |                   vbr(4)>;
; CHECK-NEXT:          |        2, 4>                |
; CHECK-NEXT:      79:1|    2: <65533, 4, 1, 2, 0, 2,|      @a1 = abbrev <2, vbr(6), vbr(6), 
; CHECK-NEXT:          |        6, 0, 2, 6, 0, 1, 4> |                   fixed(4)>;
; CHECK-NEXT:      84:4|    2: <65533, 4, 1, 3, 0, 2,|      @a2 = abbrev <3, vbr(6), 
; CHECK-NEXT:          |        6, 0, 1, 4, 0, 1, 4> |                   fixed(4), fixed(4)>;
; CHECK-NEXT:      89:7|    2: <65533, 1, 1, 10>     |      @a3 = abbrev <10>;
; CHECK-NEXT:      91:7|    2: <65533, 2, 1, 10, 0,  |      @a4 = abbrev <10, vbr(6)>;
; CHECK-NEXT:          |        2, 6>                |
; CHECK-NEXT:      95:0|    2: <65533, 1, 1, 15>     |      @a5 = abbrev <15>;
; CHECK-NEXT:      97:0|    2: <65533, 3, 1, 43, 0,  |      @a6 = abbrev <43, vbr(6), 
; CHECK-NEXT:          |        2, 6, 0, 1, 4>       |                   fixed(4)>;
; CHECK-NEXT:     101:2|    2: <65533, 4, 1, 24, 0,  |      @a7 = abbrev <24, vbr(6), vbr(6),
; CHECK-NEXT:          |        2, 6, 0, 2, 6, 0, 2, |                   vbr(4)>;
; CHECK-NEXT:          |        4>                   |
; CHECK-NEXT:     106:5|    1: <1, 19>               |    globals:
; CHECK-NEXT:     109:1|    2: <65533, 19, 3, 1, 0,  |      @a0 = abbrev <0, vbr(6), 
; CHECK-NEXT:          |        0, 2, 6, 0, 1, 1>    |                   fixed(1)>;
; CHECK-NEXT:     113:3|    2: <65533, 2, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8)>;
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:     116:4|    2: <65533, 2, 1, 2, 0, 2,|      @a2 = abbrev <2, vbr(8)>;
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:     119:5|    2: <65533, 3, 1, 3, 0, 3,|      @a3 = abbrev <3, array(fixed(8))>
; CHECK-NEXT:          |        0, 1, 8>             |          ;
; CHECK-NEXT:     123:2|    2: <65533, 2, 1, 4, 0, 2,|      @a4 = abbrev <4, vbr(6)>;
; CHECK-NEXT:          |        6>                   |
; CHECK-NEXT:     126:3|    2: <65533, 3, 1, 4, 0, 2,|      @a5 = abbrev <4, vbr(6), vbr(6)>;
; CHECK-NEXT:          |        6, 0, 2, 6>          |
; CHECK-NEXT:     130:5|  0: <65534>                 |  }
; CHECK-NEXT:     132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; CHECK-NEXT:     140:0|    2: <65533, 4, 1, 21, 0,  |    %a0 = abbrev <21, fixed(1), 
; CHECK-NEXT:          |        1, 1, 0, 3, 0, 1, 4> |                  array(fixed(4))>;
; CHECK-NEXT:     144:7|    3: <1, 11>               |    count 11;
; CHECK-NEXT:     147:4|    3: <3>                   |    @t0 = float;
; CHECK-NEXT:     149:3|    3: <4>                   |    @t1 = double;
; CHECK-NEXT:     151:2|    3: <7, 8>                |    @t2 = i8;
; CHECK-NEXT:     153:7|    3: <2>                   |    @t3 = void;
; CHECK-NEXT:     155:6|    3: <7, 16>               |    @t4 = i16;
; CHECK-NEXT:     158:3|    3: <7, 32>               |    @t5 = i32;
; CHECK-NEXT:     161:6|    3: <7, 64>               |    @t6 = i64;
; CHECK-NEXT:     165:1|    3: <7, 1>                |    @t7 = i1;
; CHECK-NEXT:     167:6|    4: <21, 0, 0>            |    @t8 = float (); <%a0>
; CHECK-NEXT:     169:4|    4: <21, 0, 1>            |    @t9 = double (); <%a0>
; CHECK-NEXT:     171:2|    4: <21, 0, 3>            |    @t10 = void (); <%a0>
; CHECK-NEXT:     173:0|  0: <65534>                 |  }
; CHECK-NEXT:     176:0|  3: <8, 10, 0, 0, 0>        |  define external void @f0();
; CHECK-NEXT:     180:6|  3: <8, 10, 0, 0, 0>        |  define external void @f1();
; CHECK-NEXT:     185:4|  3: <8, 8, 0, 0, 0>         |  define external float @f2();
; CHECK-NEXT:     190:2|  3: <8, 8, 0, 0, 0>         |  define external float @f3();
; CHECK-NEXT:     195:0|  3: <8, 8, 0, 0, 0>         |  define external float @f4();
; CHECK-NEXT:     199:6|  3: <8, 9, 0, 0, 0>         |  define external double @f5();
; CHECK-NEXT:     204:4|  3: <8, 9, 0, 0, 0>         |  define external double @f6();
; CHECK-NEXT:     209:2|  3: <8, 9, 0, 0, 0>         |  define external double @f7();
; CHECK-NEXT:     214:0|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     220:0|    3: <5, 0>                |    count 0;
; CHECK-NEXT:     222:6|  0: <65534>                 |  }
; CHECK-NEXT:     224:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     232:0|    6: <1, 0, 84, 101, 115,  |    @f0 : "TestIntegers"; <@a2>
; CHECK-NEXT:          |        116, 73, 110, 116,   |
; CHECK-NEXT:          |        101, 103, 101, 114,  |
; CHECK-NEXT:          |        115>                 |
; CHECK-NEXT:     243:1|    6: <1, 7, 71, 101, 116,  |    @f7 : "GetDoubleNegInf"; <@a2>
; CHECK-NEXT:          |        68, 111, 117, 98,    |
; CHECK-NEXT:          |        108, 101, 78, 101,   |
; CHECK-NEXT:          |        103, 73, 110, 102>   |
; CHECK-NEXT:     256:4|    6: <1, 2, 71, 101, 116,  |    @f2 : "GetFloatNan"; <@a2>
; CHECK-NEXT:          |        70, 108, 111, 97,    |
; CHECK-NEXT:          |        116, 78, 97, 110>    |
; CHECK-NEXT:     266:7|    6: <1, 3, 71, 101, 116,  |    @f3 : "GetFloatInf"; <@a2>
; CHECK-NEXT:          |        70, 108, 111, 97,    |
; CHECK-NEXT:          |        116, 73, 110, 102>   |
; CHECK-NEXT:     277:2|    6: <1, 5, 71, 101, 116,  |    @f5 : "GetDoubleNan"; <@a2>
; CHECK-NEXT:          |        68, 111, 117, 98,    |
; CHECK-NEXT:          |        108, 101, 78, 97, 110|
; CHECK-NEXT:          |        >                    |
; CHECK-NEXT:     288:3|    6: <1, 1, 84, 101, 115,  |    @f1 : "TestFloats"; <@a2>
; CHECK-NEXT:          |        116, 70, 108, 111,   |
; CHECK-NEXT:          |        97, 116, 115>        |
; CHECK-NEXT:     298:0|    6: <1, 6, 71, 101, 116,  |    @f6 : "GetDoubleInf"; <@a2>
; CHECK-NEXT:          |        68, 111, 117, 98,    |
; CHECK-NEXT:          |        108, 101, 73, 110,   |
; CHECK-NEXT:          |        102>                 |
; CHECK-NEXT:     309:1|    6: <1, 4, 71, 101, 116,  |    @f4 : "GetFloatNegInf"; <@a2>
; CHECK-NEXT:          |        70, 108, 111, 97,    |
; CHECK-NEXT:          |        116, 78, 101, 103,   |
; CHECK-NEXT:          |        73, 110, 102>        |
; CHECK-NEXT:     321:6|  0: <65534>                 |  }
; CHECK-NEXT:     324:0|  1: <65535, 12, 4>          |  function void @f0() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     332:0|    3: <1, 1>                |
; CHECK-NEXT:     334:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     344:0|      4: <1, 2>              |      i8: <@a0>
; CHECK-NEXT:     344:7|      6: <4, 0>              |        %c0 = i8 0; <@a2>
; CHECK-NEXT:     345:2|      5: <4, 10>             |        %c1 = i8 5; <@a1>
; CHECK-NEXT:     346:5|      5: <4, 11>             |        %c2 = i8 -5; <@a1>
; CHECK-NEXT:     348:0|      3: <3>                 |        %c3 = i8 undefined;
; CHECK-NEXT:     349:7|      4: <1, 4>              |      i16: <@a0>
; CHECK-NEXT:     350:6|      6: <4, 0>              |        %c4 = i16 0; <@a2>
; CHECK-NEXT:     351:1|      5: <4, 20>             |        %c5 = i16 10; <@a1>
; CHECK-NEXT:     352:4|      5: <4, 21>             |        %c6 = i16 -10; <@a1>
; CHECK-NEXT:     353:7|      3: <3>                 |        %c7 = i16 undefined;
; CHECK-NEXT:     355:6|      4: <1, 5>              |      i32: <@a0>
; CHECK-NEXT:     356:5|      6: <4, 0>              |        %c8 = i32 0; <@a2>
; CHECK-NEXT:     357:0|      5: <4, 40>             |        %c9 = i32 20; <@a1>
; CHECK-NEXT:     358:3|      5: <4, 41>             |        %c10 = i32 -20; <@a1>
; CHECK-NEXT:     359:6|      3: <3>                 |        %c11 = i32 undefined;
; CHECK-NEXT:     361:5|      4: <1, 6>              |      i64: <@a0>
; CHECK-NEXT:     362:4|      6: <4, 0>              |        %c12 = i64 0; <@a2>
; CHECK-NEXT:     362:7|      5: <4, 60>             |        %c13 = i64 30; <@a1>
; CHECK-NEXT:     364:2|      5: <4, 61>             |        %c14 = i64 -30; <@a1>
; CHECK-NEXT:     365:5|      3: <3>                 |        %c15 = i64 undefined;
; CHECK-NEXT:     367:4|      4: <1, 7>              |      i1: <@a0>
; CHECK-NEXT:     368:3|      6: <4, 0>              |        %c16 = i1 0; <@a2>
; CHECK-NEXT:     368:6|      5: <4, 3>              |        %c17 = i1 1; <@a1>
; CHECK-NEXT:     370:1|      3: <3>                 |        %c18 = i1 undefined;
; CHECK-NEXT:     372:0|    0: <65534>               |      }
; CHECK-NEXT:     376:0|    5: <2, 2, 3, 0>          |
; CHECK-NEXT:     378:4|    5: <2, 20, 20, 0>        |
; CHECK-NEXT:     381:0|    5: <2, 20, 21, 0>        |
; CHECK-NEXT:     383:4|    5: <2, 20, 22, 0>        |
; CHECK-NEXT:     386:0|    5: <2, 18, 19, 0>        |
; CHECK-NEXT:     388:4|    5: <2, 18, 20, 0>        |
; CHECK-NEXT:     391:0|    5: <2, 16, 17, 0>        |
; CHECK-NEXT:     393:4|    5: <2, 16, 18, 0>        |
; CHECK-NEXT:     396:0|    5: <2, 14, 15, 0>        |
; CHECK-NEXT:     398:4|    5: <2, 14, 16, 0>        |
; CHECK-NEXT:     401:0|    5: <2, 11, 13, 0>        |
; CHECK-NEXT:     403:4|    5: <2, 27, 30, 0>        |
; CHECK-NEXT:     406:0|    5: <2, 24, 27, 0>        |
; CHECK-NEXT:     408:4|    5: <2, 21, 24, 0>        |
; CHECK-NEXT:     411:0|    5: <2, 18, 21, 0>        |
; CHECK-NEXT:     413:4|    7: <10>                  |
; CHECK-NEXT:     414:0|  0: <65534>                 |  }
; CHECK-NEXT:     416:0|  1: <65535, 12, 4>          |  function void @f1() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     424:0|    3: <1, 1>                |
; CHECK-NEXT:     426:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     436:0|      4: <1, 0>              |      float: <@a0>
; CHECK-NEXT:     436:7|      7: <6, 0>              |        %c0 = float 0; <@a3>
; CHECK-NEXT:     438:2|      7: <6, 1065353216>     |        %c1 = float 1; <@a3>
; CHECK-NEXT:     443:5|      7: <6, 1088421888>     |        %c2 = float 7; <@a3>
; CHECK-NEXT:     449:0|      7: <6, 1090519040>     |        %c3 = float 8; <@a3>
; CHECK-NEXT:     454:3|      3: <3>                 |        %c4 = float undefined;
; CHECK-NEXT:     456:2|      4: <1, 1>              |      double: <@a0>
; CHECK-NEXT:     457:1|      7: <6,                 |        %c5 = double 6; <@a3>
; CHECK-NEXT:          |        4618441417868443648> |
; CHECK-NEXT:     466:4|      7: <6,                 |        %c6 = double 1; <@a3>
; CHECK-NEXT:          |        4607182418800017408> |
; CHECK-NEXT:     475:7|      7: <6, 0>              |        %c7 = double 0; <@a3>
; CHECK-NEXT:     477:2|      7: <6,                 |        %c8 = double 5; <@a3>
; CHECK-NEXT:          |        4617315517961601024> |
; CHECK-NEXT:     486:5|      3: <3>                 |        %c9 = double undefined;
; CHECK-NEXT:     488:4|    0: <65534>               |      }
; CHECK-NEXT:     492:0|    5: <2, 9, 10, 0>         |
; CHECK-NEXT:     494:4|    5: <2, 5, 4, 0>          |
; CHECK-NEXT:     497:0|    5: <2, 10, 9, 1>         |
; CHECK-NEXT:     499:4|    5: <2, 5, 8, 1>          |
; CHECK-NEXT:     502:0|    5: <2, 10, 14, 0>        |
; CHECK-NEXT:     504:4|    5: <2, 6, 10, 1>         |
; CHECK-NEXT:     507:0|    7: <10>                  |
; CHECK-NEXT:     507:4|  0: <65534>                 |  }
; CHECK-NEXT:     508:0|  1: <65535, 12, 4>          |  function float @f2() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     516:0|    3: <1, 1>                |
; CHECK-NEXT:     518:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     528:0|      4: <1, 0>              |      float: <@a0>
; CHECK-NEXT:     528:7|      7: <6, 2143289344>     |        %c0 = float nan; <@a3>
; CHECK-NEXT:     534:2|    0: <65534>               |      }
; CHECK-NEXT:     536:0|    8: <10, 1>               |
; CHECK-NEXT:     537:2|  0: <65534>                 |  }
; CHECK-NEXT:     540:0|  1: <65535, 12, 4>          |  function float @f3() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     548:0|    3: <1, 1>                |
; CHECK-NEXT:     550:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     560:0|      4: <1, 0>              |      float: <@a0>
; CHECK-NEXT:     560:7|      7: <6, 2139095040>     |        %c0 = float inf; <@a3>
; CHECK-NEXT:     566:2|    0: <65534>               |      }
; CHECK-NEXT:     568:0|    8: <10, 1>               |
; CHECK-NEXT:     569:2|  0: <65534>                 |  }
; CHECK-NEXT:     572:0|  1: <65535, 12, 4>          |  function float @f4() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     580:0|    3: <1, 1>                |
; CHECK-NEXT:     582:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     592:0|      4: <1, 0>              |      float: <@a0>
; CHECK-NEXT:     592:7|      7: <6, 4286578688>     |        %c0 = float -inf; <@a3>
; CHECK-NEXT:     598:2|    0: <65534>               |      }
; CHECK-NEXT:     600:0|    8: <10, 1>               |
; CHECK-NEXT:     601:2|  0: <65534>                 |  }
; CHECK-NEXT:     604:0|  1: <65535, 12, 4>          |  function double @f5() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     612:0|    3: <1, 1>                |
; CHECK-NEXT:     614:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     624:0|      4: <1, 1>              |      double: <@a0>
; CHECK-NEXT:     624:7|      7: <6,                 |        %c0 = double nan; <@a3>
; CHECK-NEXT:          |        9221120237041090560> |
; CHECK-NEXT:     634:2|    0: <65534>               |      }
; CHECK-NEXT:     636:0|    8: <10, 1>               |
; CHECK-NEXT:     637:2|  0: <65534>                 |  }
; CHECK-NEXT:     640:0|  1: <65535, 12, 4>          |  function double @f6() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     648:0|    3: <1, 1>                |
; CHECK-NEXT:     650:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     660:0|      4: <1, 1>              |      double: <@a0>
; CHECK-NEXT:     660:7|      7: <6,                 |        %c0 = double inf; <@a3>
; CHECK-NEXT:          |        9218868437227405312> |
; CHECK-NEXT:     670:2|    0: <65534>               |      }
; CHECK-NEXT:     672:0|    8: <10, 1>               |
; CHECK-NEXT:     673:2|  0: <65534>                 |  }
; CHECK-NEXT:     676:0|  1: <65535, 12, 4>          |  function double @f7() {  
; CHECK-NEXT:          |                             |                   // BlockID = 12
; CHECK-NEXT:     684:0|    3: <1, 1>                |
; CHECK-NEXT:     686:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; CHECK-NEXT:     696:0|      4: <1, 1>              |      double: <@a0>
; CHECK-NEXT:     696:7|      7: <6,                 |        %c0 = double -inf; <@a3>
; CHECK-NEXT:          |        18442240474082181120>|
; CHECK-NEXT:     707:2|    0: <65534>               |      }
; CHECK-NEXT:     708:0|    8: <10, 1>               |
; CHECK-NEXT:     709:2|  0: <65534>                 |  }
; CHECK-NEXT:     712:0|0: <65534>                   |}
