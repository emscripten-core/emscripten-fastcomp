; Test that we properly handle the globals block correctly, when it ends with
; a compound initializer.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis | FileCheck %s

@bytes = internal global [7 x i8] c"abcdefg"
@compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
declare void @func();

; CHECK:            0:0 <65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:            8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:            0>                          |
; CHECK-NEXT:      16:0 <65535, 8, 2>                |module {  // BlockID = 8
; CHECK-NEXT:      24:0   <1, 1>                     |  version 1;
; CHECK-NEXT:      26:4   <65535, 0, 2>              |  abbreviations {  // BlockID = 0
; CHECK-NEXT:      27:6   <65534>                    |  }
; CHECK-NEXT:     132:0   <65535, 17, 3>             |  types {  // BlockID = 17
; CHECK-NEXT:     149:2     <1, 2>                   |    count 2;
; CHECK-NEXT:     151:7     <2>                      |    @t0 = void;
; CHECK-NEXT:     153:6     <21, 0, 0>               |    @t1 = void ();
; CHECK-NEXT:     155:2   <65534>                    |  }
; CHECK-NEXT:     156:0   <8, 1, 0, 1, 0>            |  declare external void @f0();
; CHECK-NEXT:     160:6   <65535, 19, 4>             |  globals {  // BlockID = 19
; CHECK-NEXT:     168:0     <5, 2>                   |    count 2;
; CHECK-NEXT:     170:6     <0, 0, 0>                |    var @g0, align 0,
; CHECK-NEXT:     172:1     <3, 97, 98, 99, 100, 101,|      { 97,  98,  99, 100, 101, 102, 
; CHECK-NEXT:                102, 103>               |       103}
; CHECK-NEXT:     180:3     <0, 0, 0>                |    var @g1, align 0,
; CHECK-NEXT:     181:6     <1, 2>                   |      initializers 2 {
; CHECK-NEXT:     183:2     <3, 102, 111, 111>       |        {102, 111, 111}
; CHECK-NEXT:     187:4     <4, 0>                   |        reloc @f0;
; CHECK-NEXT:                                        |      }
; CHECK-NEXT:     188:6   <65534>                    |  }
; CHECK-NEXT:     192:0   <65535, 14, 3>             |  valuesymtab {  // BlockID = 14
; CHECK-NEXT:     200:0     <1, 2, 99, 111, 109, 112,|
; CHECK-NEXT:                111, 117, 110, 100>     |
; CHECK-NEXT:     208:1     <1, 1, 98, 121, 116, 101,|
; CHECK-NEXT:                115>                    |
; CHECK-NEXT:     214:0     <1, 0, 102, 117, 110, 99>|
; CHECK-NEXT:     219:1   <65534>                    |  }
; CHECK-NEXT:     220:0 <65534>                      |}
