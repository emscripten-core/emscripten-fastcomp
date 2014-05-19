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
; CHECK-NEXT:      27:6|  0: <65534>                 |  }
; CHECK-NEXT:     132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; CHECK-NEXT:     149:2|    3: <1, 2>                |    count 2;
; CHECK-NEXT:     151:7|    3: <2>                   |    @t0 = void;
; CHECK-NEXT:     153:6|    5: <21, 0, 0>            |    @t1 = void ();
; CHECK-NEXT:     155:2|  0: <65534>                 |  }
; CHECK-NEXT:     156:0|  3: <8, 1, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT:     160:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; CHECK-NEXT:     168:0|    3: <5, 2>                |    count 2;
; CHECK-NEXT:     170:6|    4: <0, 0, 0>             |    var @g0, align 0,
; CHECK-NEXT:     172:1|    7: <3, 97, 98, 99, 100,  |      { 97,  98,  99, 100, 101, 102, 
; CHECK-NEXT:          |        101, 102, 103>       |       103}
; CHECK-NEXT:     180:3|    4: <0, 0, 0>             |    var @g1, align 0,
; CHECK-NEXT:     181:6|    5: <1, 2>                |      initializers 2 {
; CHECK-NEXT:     183:2|    7: <3, 102, 111, 111>    |        {102, 111, 111}
; CHECK-NEXT:     187:4|    8: <4, 0>                |        reloc @f0;
; CHECK-NEXT:          |                             |      }
; CHECK-NEXT:     188:6|  0: <65534>                 |  }
; CHECK-NEXT:     192:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     200:0|    6: <1, 2, 99, 111, 109,  |    @g1 : "compound";
; CHECK-NEXT:          |        112, 111, 117, 110,  |
; CHECK-NEXT:          |        100>                 |
; CHECK-NEXT:     208:1|    6: <1, 1, 98, 121, 116,  |    @g0 : "bytes";
; CHECK-NEXT:          |        101, 115>            |
; CHECK-NEXT:     214:0|    6: <1, 0, 102, 117, 110, |    @f0 : "func";
; CHECK-NEXT:          |        99>                  |
; CHECK-NEXT:     219:1|  0: <65534>                 |  }
; CHECK-NEXT:     220:0|0: <65534>                   |}
