; Tests that forward (relocation) reference to a global works.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; CHECK:       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; CHECK-NEXT:     | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; CHECK-NEXT:     | 0>                          |
; CHECK-NEXT: 16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; CHECK-NEXT: 24:0|  3: <1, 1>                  |  version 1;
; CHECK-NEXT: 26:4|  1: <65535, 17, 2>          |  types {  // BlockID = 17
; CHECK-NEXT: 36:0|    3: <1, 2>                |    count 2;
; CHECK-NEXT: 38:4|    3: <2>                   |    @t0 = void;
; CHECK-NEXT: 40:2|    3: <21, 0, 0>            |    @t1 = void ();
; CHECK-NEXT: 43:4|  0: <65534>                 |  }

declare void @f0()

; CHECK-NEXT: 44:0|  3: <8, 1, 0, 1, 0>         |  declare external void @f0();
; CHECK-NEXT: 48:6|  1: <65535, 19, 2>          |  globals {  // BlockID = 19
; CHECK-NEXT: 56:0|    3: <5, 2>                |    count 2;

@g0 = internal global <{ i32 , i32 , i32 }>
        <{ i32 ptrtoint (void ()* @f0 to i32),
           i32 ptrtoint (<{ i32 , i32 , i32 }>* @g0 to i32),
           i32 ptrtoint ([4 x i8]* @g1 to i32) ; forward reference!
        }>, align 1

; CHECK-NEXT: 58:4|    3: <0, 1, 0>             |    var @g0, align 1,
; CHECK-NEXT: 61:6|    3: <1, 3>                |      initializers 3 {
; CHECK-NEXT: 64:2|    3: <4, 0>                |        reloc @f0;
; CHECK-NEXT: 66:6|    3: <4, 1>                |        reloc @g0;
; CHECK-NEXT: 69:2|    3: <4, 2>                |        reloc @g1;
; CHECK-NEXT:     |                             |      }

@g1 = internal global [4 x i8] zeroinitializer, align 4

; CHECK-NEXT: 71:6|    3: <0, 3, 0>             |    var @g1, align 4,
; CHECK-NEXT: 75:0|    3: <2, 4>                |      zerofill 4;
; CHECK-NEXT: 77:4|  0: <65534>                 |  }
; CHECK-NEXT: 80:0|  1: <65535, 14, 2>          |  valuesymtab {  // BlockID = 14
; CHECK-NEXT: 88:0|    3: <1, 0, 102, 48>       |    @f0 : "f0";
; CHECK-NEXT: 93:4|    3: <1, 1, 103, 48>       |    @g0 : "g0";
; CHECK-NEXT: 99:0|    3: <1, 2, 103, 49>       |    @g1 : "g1";
; CHECK-NEXT:104:4|  0: <65534>                 |  }
; CHECK-NEXT:108:0|0: <65534>                   |}
