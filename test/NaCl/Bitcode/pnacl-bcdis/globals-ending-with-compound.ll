; Test that we properly handle the globals block correctly, when it ends with
; a compound initializer.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis | FileCheck %s

@bytes = internal global [7 x i8] c"abcdefg"
@compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
declare void @func();

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
; CHECK-NEXT:          |        0, 1, 2>             |
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
; CHECK-NEXT:          |        6, 0, 1, 2, 0, 1, 4> |
; CHECK-NEXT:      89:7|    2: <65533, 1, 1, 10>     |
; CHECK-NEXT:      91:7|    2: <65533, 2, 1, 10, 0,  |
; CHECK-NEXT:          |        2, 6>                |
; CHECK-NEXT:      95:0|    2: <65533, 1, 1, 15>     |
; CHECK-NEXT:      97:0|    2: <65533, 3, 1, 43, 0,  |
; CHECK-NEXT:          |        2, 6, 0, 1, 2>       |
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
; CHECK-NEXT:          |        1, 1, 0, 3, 0, 1, 2> |
; CHECK-NEXT:     144:7|    3: <1, 2>                |    count 2;
; CHECK-NEXT:     147:4|    3: <2>                   |    @t0 = void;
; CHECK-NEXT:     149:3|    4: <21, 0, 0>            |    @t1 = void ();
; CHECK-NEXT:     150:7|  0: <65534>                 |  }
; CHECK-NEXT:     152:0|  3: <8, 1, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT:     156:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     164:0|    3: <5, 2>                |    count 2;
; CHECK-NEXT:     166:6|    4: <0, 0, 0>             |    var @g0, align 0,
; CHECK-NEXT:     168:1|    7: <3, 97, 98, 99, 100,  |      { 97,  98,  99, 100, 101, 102, 
; CHECK-NEXT:          |        101, 102, 103>       |       103}
; CHECK-NEXT:     176:3|    4: <0, 0, 0>             |    var @g1, align 0,
; CHECK-NEXT:     177:6|    5: <1, 2>                |      initializers 2 {
; CHECK-NEXT:     179:2|    7: <3, 102, 111, 111>    |        {102, 111, 111}
; CHECK-NEXT:     183:4|    8: <4, 0>                |        reloc @f0;
; CHECK-NEXT:          |                             |      }
; CHECK-NEXT:     184:6|  0: <65534>                 |  }
; CHECK-NEXT:     188:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     196:0|    6: <1, 2, 99, 111, 109,  |    @g1 : "compound";
; CHECK-NEXT:          |        112, 111, 117, 110,  |
; CHECK-NEXT:          |        100>                 |
; CHECK-NEXT:     204:1|    6: <1, 1, 98, 121, 116,  |    @g0 : "bytes";
; CHECK-NEXT:          |        101, 115>            |
; CHECK-NEXT:     210:0|    6: <1, 0, 102, 117, 110, |    @f0 : "func";
; CHECK-NEXT:          |        99>                  |
; CHECK-NEXT:     215:1|  0: <65534>                 |  }
; CHECK-NEXT:     216:0|0: <65534>                   |}
