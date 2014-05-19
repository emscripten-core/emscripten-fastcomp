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
; CHECK-NEXT:      27:6|  0: <65534>                 |  }
; CHECK-NEXT:     132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; CHECK-NEXT:     149:2|    3: <1, 2>                |    count 2;
; CHECK-NEXT:     151:7|    3: <2>                   |    @t0 = void;
; CHECK-NEXT:     153:6|    5: <21, 0, 0>            |    @t1 = void ();
; CHECK-NEXT:     155:2|  0: <65534>                 |  }
; CHECK-NEXT:     156:0|  3: <8, 1, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT:     160:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     168:0|    3: <5, 12>               |    count 12;
; CHECK-NEXT:     170:6|    4: <0, 3, 0>             |    var @g0, align 4,
; CHECK-NEXT:     172:1|    6: <2, 4>                |      zerofill 4;
; CHECK-NEXT:     173:5|    4: <0, 0, 0>             |    var @g1, align 0,
; CHECK-NEXT:     175:0|    7: <3, 97, 98, 99, 100,  |      { 97,  98,  99, 100, 101, 102, 
; CHECK-NEXT:          |        101, 102, 103>       |       103}
; CHECK-NEXT:     183:2|    4: <0, 0, 0>             |    var @g2, align 0,
; CHECK-NEXT:     184:5|    8: <4, 6>                |      reloc %v2;
; CHECK-NEXT:     185:7|    4: <0, 0, 0>             |    var @g3, align 0,
; CHECK-NEXT:     187:2|    8: <4, 0>                |      reloc @f0;
; CHECK-NEXT:     188:4|    4: <0, 0, 0>             |    var @g4, align 0,
; CHECK-NEXT:     189:7|    5: <1, 2>                |      initializers 2 {
; CHECK-NEXT:     191:3|    7: <3, 102, 111, 111>    |        {102, 111, 111}
; CHECK-NEXT:     195:5|    8: <4, 0>                |        reloc @f0;
; CHECK-NEXT:          |                             |      }
; CHECK-NEXT:     196:7|    4: <0, 0, 0>             |    var @g5, align 0,
; CHECK-NEXT:     198:2|    8: <4, 2>                |      reloc @g1;
; CHECK-NEXT:     199:4|    4: <0, 0, 0>             |    var @g6, align 0,
; CHECK-NEXT:     200:7|    9: <4, 6, 1>             |      reloc @g5 + 1;
; CHECK-NEXT:     202:7|    4: <0, 0, 0>             |    var @g7, align 0,
; CHECK-NEXT:     204:2|    9: <4, 2, 1>             |      reloc @g1 + 1;
; CHECK-NEXT:     206:2|    4: <0, 0, 0>             |    var @g8, align 0,
; CHECK-NEXT:     207:5|    9: <4, 2, 7>             |      reloc @g1 + 7;
; CHECK-NEXT:     209:5|    4: <0, 0, 0>             |    var @g9, align 0,
; CHECK-NEXT:     211:0|    9: <4, 2, 9>             |      reloc @g1 + 9;
; CHECK-NEXT:     213:0|    4: <0, 0, 0>             |    var @g10, align 0,
; CHECK-NEXT:     214:3|    9: <4, 5, 1>             |      reloc @g4 + 1;
; CHECK-NEXT:     216:3|    4: <0, 0, 0>             |    var @g11, align 0,
; CHECK-NEXT:     217:6|    9: <4, 5, 4>             |      reloc @g4 + 4;
; CHECK-NEXT:     219:6|  0: <65534>                 |  }
; CHECK-NEXT:     224:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     232:0|    6: <1, 1, 122, 101, 114, |    @g0 : "zero";
; CHECK-NEXT:          |        111>                 |
; CHECK-NEXT:     237:1|    6: <1, 4, 112, 116, 114, |    @g3 : "ptr_to_func";
; CHECK-NEXT:          |        95, 116, 111, 95,    |
; CHECK-NEXT:          |        102, 117, 110, 99>   |
; CHECK-NEXT:     247:4|    6: <1, 5, 99, 111, 109,  |    @g4 : "compound";
; CHECK-NEXT:          |        112, 111, 117, 110,  |
; CHECK-NEXT:          |        100>                 |
; CHECK-NEXT:     255:5|    6: <1, 2, 98, 121, 116,  |    @g1 : "bytes";
; CHECK-NEXT:          |        101, 115>            |
; CHECK-NEXT:     261:4|    6: <1, 0, 102, 117, 110, |    @f0 : "func";
; CHECK-NEXT:          |        99>                  |
; CHECK-NEXT:     266:5|    6: <1, 3, 112, 116, 114, |    @g2 : "ptr_to_ptr";
; CHECK-NEXT:          |        95, 116, 111, 95,    |
; CHECK-NEXT:          |        112, 116, 114>       |
; CHECK-NEXT:     276:2|    6: <1, 8, 97, 100, 100,  |    @g7 : "addend_array1";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        97, 114, 114, 97,    |
; CHECK-NEXT:          |        121, 49>             |
; CHECK-NEXT:     288:1|    6: <1, 9, 97, 100, 100,  |    @g8 : "addend_array2";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        97, 114, 114, 97,    |
; CHECK-NEXT:          |        121, 50>             |
; CHECK-NEXT:     300:0|    6: <1, 10, 97, 100, 100, |    @g9 : "addend_array3";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        97, 114, 114, 97,    |
; CHECK-NEXT:          |        121, 51>             |
; CHECK-NEXT:     311:7|    6: <1, 7, 97, 100, 100,  |    @g6 : "addend_ptr";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        112, 116, 114>       |
; CHECK-NEXT:     321:4|    6: <1, 11, 97, 100, 100, |    @g10 : "addend_struct1";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        115, 116, 114, 117,  |
; CHECK-NEXT:          |        99, 116, 49>         |
; CHECK-NEXT:     334:1|    6: <1, 6, 112, 116, 114> |    @g5 : "ptr";
; CHECK-NEXT:     338:4|    6: <1, 12, 97, 100, 100, |    @g11 : "addend_struct2";
; CHECK-NEXT:          |        101, 110, 100, 95,   |
; CHECK-NEXT:          |        115, 116, 114, 117,  |
; CHECK-NEXT:          |        99, 116, 50>         |
; CHECK-NEXT:     351:1|  0: <65534>                 |  }
; CHECK-NEXT:     352:0|0: <65534>                   |}
