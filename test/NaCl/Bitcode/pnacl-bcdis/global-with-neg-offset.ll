; Show that we detect when a negative offset is used within a relocation.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis  | FileCheck %s

@bytes = internal global [7 x i8] c"abcdefg"
@addend_negative = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 -1)

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
; CHECK-NEXT:          |        0, 1, 0, 0>          |
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
; CHECK-NEXT:          |        6, 0, 1, 0, 0, 0, 1, |
; CHECK-NEXT:          |        4>                   |
; CHECK-NEXT:      89:7|    2: <65533, 1, 1, 10>     |
; CHECK-NEXT:      91:7|    2: <65533, 2, 1, 10, 0,  |
; CHECK-NEXT:          |        2, 6>                |
; CHECK-NEXT:      95:0|    2: <65533, 1, 1, 15>     |
; CHECK-NEXT:      97:0|    2: <65533, 3, 1, 43, 0,  |
; CHECK-NEXT:          |        2, 6, 0, 1, 0, 0>    |
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
; CHECK-NEXT:     140:0|    2: <65533, 3, 1, 8, 0, 1,|
; CHECK-NEXT:          |        0, 0, 1, 0>          |
; CHECK-NEXT:     144:3|    2: <65533, 4, 1, 21, 0,  |
; CHECK-NEXT:          |        1, 1, 0, 3, 0, 1, 0, |
; CHECK-NEXT:          |        0>                   |
; CHECK-NEXT:     149:2|    3: <1, 0>                |    count 0;
; CHECK-NEXT:     151:7|  0: <65534>                 |  }
; CHECK-NEXT:     156:0|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     164:0|    3: <5, 2>                |    count 2;
; CHECK-NEXT:     166:6|    4: <0, 0, 0>             |    var @g0, align 0,
; CHECK-NEXT:     168:1|    7: <3, 97, 98, 99, 100,  |      { 97,  98,  99, 100, 101, 102, 
; CHECK-NEXT:          |        101, 102, 103>       |       103}
; CHECK-NEXT:     176:3|    4: <0, 0, 0>             |    var @g1, align 0,
; CHECK-NEXT:     177:6|    9: <4, 0, 4294967295>    |      reloc @g0 - 1;
; CHECK-NEXT:     184:2|  0: <65534>                 |  }
; CHECK-NEXT:     188:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     196:0|    6: <1, 1, 97, 100, 100,  |    @g1 : "addend_negative";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        110, 101, 103, 97,   |
; CHECK-NEXT:          |        116, 105, 118, 101>  |
; CHECK-NEXT:     209:3|    6: <1, 0, 98, 121, 116,  |    @g0 : "bytes";
; CHECK-NEXT:          |        101, 115>            |
; CHECK-NEXT:     215:2|  0: <65534>                 |  }
; CHECK-NEXT:     216:0|0: <65534>                   |}
