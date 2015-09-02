; Simple test to see if we handle global variables.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis | FileCheck %s


@zero = internal global [4 x i8] zeroinitializer, align 4
@bytes = internal global [7 x i8] c"abcdefg"
@ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)
@ptr_to_func = internal global i32 ptrtoint (void ()* @func to i32)
@compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
@ptr = internal global i32 ptrtoint ([7 x i8]* @bytes to i32)
@addend_ptr = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)
@addend_array1 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 1)
@addend_array2 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 7)
@addend_array3 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 9)
@addend_struct1 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)
@addend_struct2 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)

declare void @func();

; CHECK:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:          | 0>                          |
; CHECK-NEXT:      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; CHECK-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; CHECK-NEXT:      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; CHECK-NEXT:      36:0|    3: <1, 14>               |    valuesymtab:
; CHECK-NEXT:      38:4|    2: <65533, 4, 0, 1, 3, 0,|      @a0 = abbrev <fixed(3), vbr(8), 
; CHECK-NEXT:          |        2, 8, 0, 3, 0, 1, 8> |                   array(fixed(8))>;
; CHECK-NEXT:      43:2|    2: <65533, 4, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8), 
; CHECK-NEXT:          |        8, 0, 3, 0, 1, 7>    |                   array(fixed(7))>;
; CHECK-NEXT:      48:0|    2: <65533, 4, 1, 1, 0, 2,|      @a2 = abbrev <1, vbr(8), 
; CHECK-NEXT:          |        8, 0, 3, 0, 4>       |                   array(char6)>;
; CHECK-NEXT:      52:1|    2: <65533, 4, 1, 2, 0, 2,|      @a3 = abbrev <2, vbr(8), 
; CHECK-NEXT:          |        8, 0, 3, 0, 4>       |                   array(char6)>;
; CHECK-NEXT:      56:2|    3: <1, 11>               |    constants:
; CHECK-NEXT:      58:6|    2: <65533, 2, 1, 1, 0, 1,|      @a0 = abbrev <1, fixed(2)>;
; CHECK-NEXT:          |        2>                   |
; CHECK-NEXT:      61:7|    2: <65533, 2, 1, 4, 0, 2,|      @a1 = abbrev <4, vbr(8)>;
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:      65:0|    2: <65533, 2, 1, 4, 1, 0>|      @a2 = abbrev <4, 0>;
; CHECK-NEXT:      68:1|    2: <65533, 2, 1, 6, 0, 2,|      @a3 = abbrev <6, vbr(8)>;
; CHECK-NEXT:          |        8>                   |
; CHECK-NEXT:      71:2|    3: <1, 12>               |    function:
; CHECK-NEXT:      73:6|    2: <65533, 4, 1, 20, 0,  |      @a0 = abbrev <20, vbr(6), vbr(4),
; CHECK-NEXT:          |        2, 6, 0, 2, 4, 0, 2, |                   vbr(4)>;
; CHECK-NEXT:          |        4>                   |
; CHECK-NEXT:      79:1|    2: <65533, 4, 1, 2, 0, 2,|      @a1 = abbrev <2, vbr(6), vbr(6), 
; CHECK-NEXT:          |        6, 0, 2, 6, 0, 1, 4> |                   fixed(4)>;
; CHECK-NEXT:      84:4|    2: <65533, 4, 1, 3, 0, 2,|      @a2 = abbrev <3, vbr(6), 
; CHECK-NEXT:          |        6, 0, 1, 2, 0, 1, 4> |                   fixed(2), fixed(4)>;
; CHECK-NEXT:      89:7|    2: <65533, 1, 1, 10>     |      @a3 = abbrev <10>;
; CHECK-NEXT:      91:7|    2: <65533, 2, 1, 10, 0,  |      @a4 = abbrev <10, vbr(6)>;
; CHECK-NEXT:          |        2, 6>                |
; CHECK-NEXT:      95:0|    2: <65533, 1, 1, 15>     |      @a5 = abbrev <15>;
; CHECK-NEXT:      97:0|    2: <65533, 3, 1, 43, 0,  |      @a6 = abbrev <43, vbr(6), 
; CHECK-NEXT:          |        2, 6, 0, 1, 2>       |                   fixed(2)>;
; CHECK-NEXT:     101:2|    2: <65533, 4, 1, 24, 0,  |      @a7 = abbrev <24, vbr(6), vbr(6),
; CHECK-NEXT:          |        2, 6, 0, 2, 6, 0, 2, |                   vbr(4)>;
; CHECK-NEXT:          |        4>                   |
; CHECK-NEXT:     106:5|    3: <1, 19>               |    globals:
; CHECK-NEXT:     109:1|    2: <65533, 3, 1, 0, 0, 2,|      @a0 = abbrev <0, vbr(6), 
; CHECK-NEXT:          |        6, 0, 1, 1>          |                   fixed(1)>;
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
; CHECK-NEXT:          |        1, 1, 0, 3, 0, 1, 2> |                  array(fixed(2))>;
; CHECK-NEXT:     144:7|    3: <1, 2>                |    count 2;
; CHECK-NEXT:     147:4|    3: <2>                   |    @t0 = void;
; CHECK-NEXT:     149:3|    4: <21, 0, 0>            |    @t1 = void (); <%a0>
; CHECK-NEXT:     150:7|  0: <65534>                 |  }
; CHECK-NEXT:     152:0|  3: <8, 1, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT:     156:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     164:0|    3: <5, 12>               |    count 12;
; CHECK-NEXT:     166:6|    4: <0, 3, 0>             |    var @g0, align 4, <@a0>
; CHECK-NEXT:     168:1|    6: <2, 4>                |      zerofill 4; <@a2>
; CHECK-NEXT:     169:5|    4: <0, 0, 0>             |    var @g1, align 0, <@a0>
; CHECK-NEXT:     171:0|    7: <3, 97, 98, 99, 100,  |      { 97,  98,  99, 100, 101, 102, 
; CHECK-NEXT:          |        101, 102, 103>       |       103} <@a3>
; CHECK-NEXT:     179:2|    4: <0, 0, 0>             |    var @g2, align 0, <@a0>
; CHECK-NEXT:     180:5|    8: <4, 6>                |      reloc @g5; <@a4>
; CHECK-NEXT:     181:7|    4: <0, 0, 0>             |    var @g3, align 0, <@a0>
; CHECK-NEXT:     183:2|    8: <4, 0>                |      reloc @f0; <@a4>
; CHECK-NEXT:     184:4|    4: <0, 0, 0>             |    var @g4, align 0, <@a0>
; CHECK-NEXT:     185:7|    5: <1, 2>                |      initializers 2 { <@a1>
; CHECK-NEXT:     187:3|    7: <3, 102, 111, 111>    |        {102, 111, 111} <@a3>
; CHECK-NEXT:     191:5|    8: <4, 0>                |        reloc @f0; <@a4>
; CHECK-NEXT:          |                             |      }
; CHECK-NEXT:     192:7|    4: <0, 0, 0>             |    var @g5, align 0, <@a0>
; CHECK-NEXT:     194:2|    8: <4, 2>                |      reloc @g1; <@a4>
; CHECK-NEXT:     195:4|    4: <0, 0, 0>             |    var @g6, align 0, <@a0>
; CHECK-NEXT:     196:7|    9: <4, 6, 1>             |      reloc @g5 + 1; <@a5>
; CHECK-NEXT:     198:7|    4: <0, 0, 0>             |    var @g7, align 0, <@a0>
; CHECK-NEXT:     200:2|    9: <4, 2, 1>             |      reloc @g1 + 1; <@a5>
; CHECK-NEXT:     202:2|    4: <0, 0, 0>             |    var @g8, align 0, <@a0>
; CHECK-NEXT:     203:5|    9: <4, 2, 7>             |      reloc @g1 + 7; <@a5>
; CHECK-NEXT:     205:5|    4: <0, 0, 0>             |    var @g9, align 0, <@a0>
; CHECK-NEXT:     207:0|    9: <4, 2, 9>             |      reloc @g1 + 9; <@a5>
; CHECK-NEXT:     209:0|    4: <0, 0, 0>             |    var @g10, align 0, <@a0>
; CHECK-NEXT:     210:3|    9: <4, 5, 1>             |      reloc @g4 + 1; <@a5>
; CHECK-NEXT:     212:3|    4: <0, 0, 0>             |    var @g11, align 0, <@a0>
; CHECK-NEXT:     213:6|    9: <4, 5, 4>             |      reloc @g4 + 4; <@a5>
; CHECK-NEXT:     215:6|  0: <65534>                 |  }
; CHECK-NEXT:     220:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     228:0|    6: <1, 1, 122, 101, 114, |    @g0 : "zero"; <@a2>
; CHECK-NEXT:          |        111>                 |
; CHECK-NEXT:     233:1|    6: <1, 4, 112, 116, 114, |    @g3 : "ptr_to_func"; <@a2>
; CHECK-NEXT:          |        95, 116, 111, 95,    |
; CHECK-NEXT:          |        102, 117, 110, 99>   |
; CHECK-NEXT:     243:4|    6: <1, 5, 99, 111, 109,  |    @g4 : "compound"; <@a2>
; CHECK-NEXT:          |        112, 111, 117, 110,  |
; CHECK-NEXT:          |        100>                 |
; CHECK-NEXT:     251:5|    6: <1, 2, 98, 121, 116,  |    @g1 : "bytes"; <@a2>
; CHECK-NEXT:          |        101, 115>            |
; CHECK-NEXT:     257:4|    6: <1, 0, 102, 117, 110, |    @f0 : "func"; <@a2>
; CHECK-NEXT:          |        99>                  |
; CHECK-NEXT:     262:5|    6: <1, 3, 112, 116, 114, |    @g2 : "ptr_to_ptr"; <@a2>
; CHECK-NEXT:          |        95, 116, 111, 95,    |
; CHECK-NEXT:          |        112, 116, 114>       |
; CHECK-NEXT:     272:2|    6: <1, 8, 97, 100, 100,  |    @g7 : "addend_array1"; <@a2>
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        97, 114, 114, 97,    |
; CHECK-NEXT:          |        121, 49>             |
; CHECK-NEXT:     284:1|    6: <1, 9, 97, 100, 100,  |    @g8 : "addend_array2"; <@a2>
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        97, 114, 114, 97,    |
; CHECK-NEXT:          |        121, 50>             |
; CHECK-NEXT:     296:0|    6: <1, 10, 97, 100, 100, |    @g9 : "addend_array3"; <@a2>
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        97, 114, 114, 97,    |
; CHECK-NEXT:          |        121, 51>             |
; CHECK-NEXT:     307:7|    6: <1, 7, 97, 100, 100,  |    @g6 : "addend_ptr"; <@a2>
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        112, 116, 114>       |
; CHECK-NEXT:     317:4|    6: <1, 11, 97, 100, 100, |    @g10 : "addend_struct1"; <@a2>
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        115, 116, 114, 117,  |
; CHECK-NEXT:          |        99, 116, 49>         |
; CHECK-NEXT:     330:1|    6: <1, 6, 112, 116, 114> |    @g5 : "ptr"; <@a2>
; CHECK-NEXT:     334:4|    6: <1, 12, 97, 100, 100, |    @g11 : "addend_struct2"; <@a2>
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        115, 116, 114, 117,  |
; CHECK-NEXT:          |        99, 116, 50>         |
; CHECK-NEXT:     347:1|  0: <65534>                 |  }
; CHECK-NEXT:     348:0|0: <65534>                   |}
