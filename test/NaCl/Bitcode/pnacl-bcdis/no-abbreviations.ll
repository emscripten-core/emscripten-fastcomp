; Simple test to show the difference between no abbreviations, and
; the ones provided by pnacl-freeze. Differences are show by the
; addresses for records.

; TODO(kschimpf): Add basic block symbol table entry, removing errors.

; RUN: llvm-as < %s | pnacl-freeze --allow-local-symbol-tables \
; RUN:              | not pnacl-bcdis --allow-local-symbol-tables \
; RUN:              | FileCheck %s --check-prefix ABV

; RUN: llvm-as < %s | pnacl-freeze --allow-local-symbol-tables \
; RUN:              | pnacl-bccompress --remove-abbreviations \
; RUN:              | not pnacl-bcdis --allow-local-symbol-tables \
; RUN:              | FileCheck %s --check-prefix NOABV

define i32 @fact(i32 %n) {
  %v1 = icmp eq i32 0, 1
  br i1 %v1, label %true, label %false
true:
  ret i32 1
false:
  %v2 = sub i32 %n, 1
  %v3 = mul i32 %n, %v2
  %v4 = call i32 @fact(i32 %v3)
  ret i32 %v4
}
 
; ABV:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; ABV-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; ABV-NEXT:          | 0>                          |
; ABV-NEXT:      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; ABV-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; ABV-NEXT:      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; ABV-NEXT:      27:6|  0: <65534>                 |  }
; ABV-NEXT:     132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; ABV-NEXT:     149:2|    3: <1, 4>                |    count 4;
; ABV-NEXT:     151:7|    3: <7, 32>               |    @t0 = i32;
; ABV-NEXT:     155:2|    3: <2>                   |    @t1 = void;
; ABV-NEXT:     157:1|    5: <21, 0, 0, 0>         |    @t2 = i32 (i32);
; ABV-NEXT:     159:1|    3: <7, 1>                |    @t3 = i1;
; ABV-NEXT:     161:6|  0: <65534>                 |  }
; ABV-NEXT:     164:0|  3: <8, 2, 0, 0, 0>         |  define external i32 @f0(i32);
; ABV-NEXT:     168:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; ABV-NEXT:     176:0|    3: <5, 0>                |    count 0;
; ABV-NEXT:     178:6|  0: <65534>                 |  }
; ABV-NEXT:     180:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; ABV-NEXT:     188:0|    6: <1, 0, 102, 97, 99,   |    @f0 : "fact";
; ABV-NEXT:          |        116>                 |
; ABV-NEXT:     193:1|  0: <65534>                 |  }
; ABV-NEXT:     196:0|  1: <65535, 12, 4>          |  function i32 @f0(i32 %p0) {  
; ABV-NEXT:          |                             |                   // BlockID = 12
; ABV-NEXT:     204:0|    3: <1, 3>                |
; ABV-NEXT:     206:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; ABV-NEXT:     216:0|      4: <1, 0>              |
; ABV-NEXT:     216:6|      5: <4, 2>              |
; ABV-NEXT:     218:1|      6: <4, 0>              |
; ABV-NEXT:     218:4|    0: <65534>               |    }
; ABV-NEXT:     220:0|    3: <28, 1, 2, 32>        |
; ABV-NEXT:     225:0|    3: <11, 1, 2, 1>         |
; ABV-NEXT:     229:2|    8: <10, 3>               |
; ABV-NEXT:     230:4|    5: <2, 4, 3, 1>          |
; ABV-NEXT:     233:0|    5: <2, 5, 1, 2>          |
; ABV-NEXT:     235:4|    3: <34, 0, 7, 1>         |
; ABV-NEXT:     240:4|    8: <10, 1>               |
; ABV-NEXT:     241:6|    1: <65535, 14, 3>        |    valuesymtab {  // BlockID = 14
; ABV-NEXT:     248:0|      7: <2, 1, 116, 114,    |
; ABV-NEXT:          |        117, 101>            |
; ABV-NEXT:Error(248:0): Unknown record in valuesymtab block.
; ABV-NEXT:     253:1|      6: <1, 4, 118, 49>     |      %v2 : "v1";
; ABV-NEXT:     256:6|      6: <1, 5, 118, 50>     |      %v3 : "v2";
; ABV-NEXT:     260:3|      6: <1, 6, 118, 51>     |      %v4 : "v3";
; ABV-NEXT:     264:0|      6: <1, 7, 118, 52>     |      %v5 : "v4";
; ABV-NEXT:     267:5|      7: <2, 2, 102, 97, 108,|
; ABV-NEXT:          |        115, 101>            |
; ABV-NEXT:Error(267:5): Unknown record in valuesymtab block.
; ABV-NEXT:     273:4|      6: <1, 1, 110>         |      %p0 : "n";
; ABV-NEXT:     276:3|    0: <65534>               |    }
; ABV-NEXT:     280:0|  0: <65534>                 |  }
; ABV-NEXT:     284:0|0: <65534>                   |}

; NOABV:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; NOABV-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; NOABV-NEXT:          | 0>                          |
; NOABV-NEXT:      16:0|1: <65535, 8, 3>             |module {  // BlockID = 8
; NOABV-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; NOABV-NEXT:      26:5|  1: <65535, 0, 3>           |  abbreviations {  // BlockID = 0
; NOABV-NEXT:      28:0|  0: <65534>                 |  }
; NOABV-NEXT:      40:0|  1: <65535, 17, 4>          |  types {  // BlockID = 17
; NOABV-NEXT:      48:0|    3: <1, 4>                |    count 4;
; NOABV-NEXT:      50:6|    3: <7, 32>               |    @t0 = i32;
; NOABV-NEXT:      54:2|    3: <2>                   |    @t1 = void;
; NOABV-NEXT:      56:2|    3: <21, 0, 0, 0>         |    @t2 = i32 (i32);
; NOABV-NEXT:      60:4|    3: <7, 1>                |    @t3 = i1;
; NOABV-NEXT:      63:2|  0: <65534>                 |  }
; NOABV-NEXT:      64:0|  3: <8, 2, 0, 0, 0>         |  define external i32 @f0(i32);
; NOABV-NEXT:      68:7|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; NOABV-NEXT:      76:0|    3: <5, 0>                |    count 0;
; NOABV-NEXT:      78:6|  0: <65534>                 |  }
; NOABV-NEXT:      80:0|  1: <65535, 14, 4>          |  valuesymtab {  // BlockID = 14
; NOABV-NEXT:      88:0|    3: <1, 0, 102, 97, 99,   |    @f0 : "fact";
; NOABV-NEXT:          |        116>                 |
; NOABV-NEXT:      96:6|  0: <65534>                 |  }
; NOABV-NEXT:     100:0|  1: <65535, 12, 5>          |  function i32 @f0(i32 %p0) {  
; NOABV-NEXT:          |                             |                   // BlockID = 12
; NOABV-NEXT:     108:0|    3: <1, 3>                |
; NOABV-NEXT:     110:7|    1: <65535, 11, 4>        |    constants {  // BlockID = 11
; NOABV-NEXT:     120:0|      3: <1, 0>              |
; NOABV-NEXT:     122:6|      3: <4, 2>              |
; NOABV-NEXT:     125:4|      3: <4, 0>              |
; NOABV-NEXT:     128:2|    0: <65534>               |    }
; NOABV-NEXT:     132:0|    3: <28, 1, 2, 32>        |
; NOABV-NEXT:     137:1|    3: <11, 1, 2, 1>         |
; NOABV-NEXT:     141:4|    3: <10, 3>               |
; NOABV-NEXT:     144:3|    3: <2, 4, 3, 1>          |
; NOABV-NEXT:     148:6|    3: <2, 5, 1, 2>          |
; NOABV-NEXT:     153:1|    3: <34, 0, 7, 1>         |
; NOABV-NEXT:     158:2|    3: <10, 1>               |
; NOABV-NEXT:     161:1|    1: <65535, 14, 4>        |    valuesymtab {  // BlockID = 14
; NOABV-NEXT:     168:0|      3: <2, 1, 116, 114,    |
; NOABV-NEXT:          |        117, 101>            |
; NOABV-NEXT:Error(168:0): Unknown record in valuesymtab block.
; NOABV-NEXT:     176:6|      3: <1, 4, 118, 49>     |      %v2 : "v1";
; NOABV-NEXT:     182:4|      3: <1, 5, 118, 50>     |      %v3 : "v2";
; NOABV-NEXT:     188:2|      3: <1, 6, 118, 51>     |      %v4 : "v3";
; NOABV-NEXT:     194:0|      3: <1, 7, 118, 52>     |      %v5 : "v4";
; NOABV-NEXT:     199:6|      3: <2, 2, 102, 97, 108,|
; NOABV-NEXT:          |        115, 101>            |
; NOABV-NEXT:Error(199:6): Unknown record in valuesymtab block.
; NOABV-NEXT:     210:0|      3: <1, 1, 110>         |      %p0 : "n";
; NOABV-NEXT:     214:2|    0: <65534>               |    }
; NOABV-NEXT:     216:0|  0: <65534>                 |  }
; NOABV-NEXT:     220:0|0: <65534>                   |}
