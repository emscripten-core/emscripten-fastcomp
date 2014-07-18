; Simple test to show the difference between no abbreviations, and
; the ones provided by pnacl-freeze. Differences are show by the
; addresses for records.

; RUN: llvm-as < %s | pnacl-freeze --allow-local-symbol-tables \
; RUN:              | pnacl-bcdis --allow-local-symbol-tables \
; RUN:              | FileCheck %s --check-prefix ABV

; RUN: llvm-as < %s | pnacl-freeze --allow-local-symbol-tables \
; RUN:              | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis --allow-local-symbol-tables \
; RUN:              | FileCheck %s --check-prefix NOABV

define i32 @fact(i32 %n) {
  %v1 = icmp eq i32 0, 1
  br i1 %v1, label %true, label %false
true:
  ret i32 1
false:
  %v2 = sub i32 %n, 1
  %v3 = call i32 @fact(i32 %v2)
  %v4 = mul i32 %n, %v3
  ret i32 %v4
}

; ABV:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; ABV-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; ABV-NEXT:          | 0>                          |
; ABV-NEXT:      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; ABV-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; ABV-NEXT:      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; ABV-NEXT:      36:0|    1: <1, 14>               |    valuesymtab:
; ABV-NEXT:      38:4|    2: <65533, 4, 0, 1, 3, 0,|      @a0 = abbrev <fixed(3), vbr(8), 
; ABV-NEXT:          |        2, 8, 0, 3, 0, 1, 8> |                   array(fixed(8))>;
; ABV-NEXT:      43:2|    2: <65533, 4, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8), 
; ABV-NEXT:          |        8, 0, 3, 0, 1, 7>    |                   array(fixed(7))>;
; ABV-NEXT:      48:0|    2: <65533, 4, 1, 1, 0, 2,|      @a2 = abbrev <1, vbr(8), 
; ABV-NEXT:          |        8, 0, 3, 0, 4>       |                   array(char6)>;
; ABV-NEXT:      52:1|    2: <65533, 4, 1, 2, 0, 2,|      @a3 = abbrev <2, vbr(8), 
; ABV-NEXT:          |        8, 0, 3, 0, 4>       |                   array(char6)>;
; ABV-NEXT:      56:2|    1: <1, 11>               |    constants:
; ABV-NEXT:      58:6|    2: <65533, 2, 1, 1, 0, 1,|      @a0 = abbrev <1, fixed(3)>;
; ABV-NEXT:          |        3>                   |
; ABV-NEXT:      61:7|    2: <65533, 2, 1, 4, 0, 2,|      @a1 = abbrev <4, vbr(8)>;
; ABV-NEXT:          |        8>                   |
; ABV-NEXT:      65:0|    2: <65533, 2, 1, 4, 1, 0>|      @a2 = abbrev <4, 0>;
; ABV-NEXT:      68:1|    2: <65533, 2, 1, 6, 0, 2,|      @a3 = abbrev <6, vbr(8)>;
; ABV-NEXT:          |        8>                   |
; ABV-NEXT:      71:2|    1: <1, 12>               |    function:
; ABV-NEXT:      73:6|    2: <65533, 4, 1, 20, 0,  |      @a0 = abbrev <20, vbr(6), vbr(4),
; ABV-NEXT:          |        2, 6, 0, 2, 4, 0, 2, |                   vbr(4)>;
; ABV-NEXT:          |        4>                   |
; ABV-NEXT:      79:1|    2: <65533, 4, 1, 2, 0, 2,|      @a1 = abbrev <2, vbr(6), vbr(6), 
; ABV-NEXT:          |        6, 0, 2, 6, 0, 1, 4> |                   fixed(4)>;
; ABV-NEXT:      84:4|    2: <65533, 4, 1, 3, 0, 2,|      @a2 = abbrev <3, vbr(6), 
; ABV-NEXT:          |        6, 0, 1, 3, 0, 1, 4> |                   fixed(3), fixed(4)>;
; ABV-NEXT:      89:7|    2: <65533, 1, 1, 10>     |      @a3 = abbrev <10>;
; ABV-NEXT:      91:7|    2: <65533, 2, 1, 10, 0,  |      @a4 = abbrev <10, vbr(6)>;
; ABV-NEXT:          |        2, 6>                |
; ABV-NEXT:      95:0|    2: <65533, 1, 1, 15>     |      @a5 = abbrev <15>;
; ABV-NEXT:      97:0|    2: <65533, 3, 1, 43, 0,  |      @a6 = abbrev <43, vbr(6), 
; ABV-NEXT:          |        2, 6, 0, 1, 3>       |                   fixed(3)>;
; ABV-NEXT:     101:2|    2: <65533, 4, 1, 24, 0,  |      @a7 = abbrev <24, vbr(6), vbr(6),
; ABV-NEXT:          |        2, 6, 0, 2, 6, 0, 2, |                   vbr(4)>;
; ABV-NEXT:          |        4>                   |
; ABV-NEXT:     106:5|    1: <1, 19>               |    globals:
; ABV-NEXT:     109:1|    2: <65533, 3, 1, 0, 0, 2,|      @a0 = abbrev <0, vbr(6), 
; ABV-NEXT:          |        6, 0, 1, 1>          |                   fixed(1)>;
; ABV-NEXT:     113:3|    2: <65533, 2, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8)>;
; ABV-NEXT:          |        8>                   |
; ABV-NEXT:     116:4|    2: <65533, 2, 1, 2, 0, 2,|      @a2 = abbrev <2, vbr(8)>;
; ABV-NEXT:          |        8>                   |
; ABV-NEXT:     119:5|    2: <65533, 3, 1, 3, 0, 3,|      @a3 = abbrev <3, array(fixed(8))>
; ABV-NEXT:          |        0, 1, 8>             |          ;
; ABV-NEXT:     123:2|    2: <65533, 2, 1, 4, 0, 2,|      @a4 = abbrev <4, vbr(6)>;
; ABV-NEXT:          |        6>                   |
; ABV-NEXT:     126:3|    2: <65533, 3, 1, 4, 0, 2,|      @a5 = abbrev <4, vbr(6), vbr(6)>;
; ABV-NEXT:          |        6, 0, 2, 6>          |
; ABV-NEXT:     130:5|  0: <65534>                 |  }
; ABV-NEXT:     132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; ABV-NEXT:     140:0|    2: <65533, 4, 1, 21, 0,  |    %a0 = abbrev <21, fixed(1), 
; ABV-NEXT:          |        1, 1, 0, 3, 0, 1, 3> |                  array(fixed(3))>;
; ABV-NEXT:     144:7|    3: <1, 4>                |    count 4;
; ABV-NEXT:     147:4|    3: <7, 32>               |    @t0 = i32;
; ABV-NEXT:     150:7|    3: <2>                   |    @t1 = void;
; ABV-NEXT:     152:6|    4: <21, 0, 0, 0>         |    @t2 = i32 (i32); <%a0>
; ABV-NEXT:     154:6|    3: <7, 1>                |    @t3 = i1;
; ABV-NEXT:     157:3|  0: <65534>                 |  }
; ABV-NEXT:     160:0|  3: <8, 2, 0, 0, 0>         |  define external i32 @f0(i32);
; ABV-NEXT:     164:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; ABV-NEXT:     172:0|    3: <5, 0>                |    count 0;
; ABV-NEXT:     174:6|  0: <65534>                 |  }
; ABV-NEXT:     176:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; ABV-NEXT:     184:0|    6: <1, 0, 102, 97, 99,   |    @f0 : "fact"; <@a2>
; ABV-NEXT:          |        116>                 |
; ABV-NEXT:     189:1|  0: <65534>                 |  }
; ABV-NEXT:     192:0|  1: <65535, 12, 4>          |  function i32 @f0(i32 %p0) {  
; ABV-NEXT:          |                             |                   // BlockID = 12
; ABV-NEXT:     200:0|    3: <1, 3>                |    blocks 3;
; ABV-NEXT:     202:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; ABV-NEXT:     212:0|      4: <1, 0>              |      i32: <@a0>
; ABV-NEXT:     212:6|      5: <4, 2>              |        %c0 = i32 1; <@a1>
; ABV-NEXT:     214:1|      6: <4, 0>              |        %c1 = i32 0; <@a2>
; ABV-NEXT:     214:4|    0: <65534>               |      }
; ABV-NEXT:          |                             |  %b0:
; ABV-NEXT:     216:0|    3: <28, 1, 2, 32>        |    %v0 = icmp eq i32 %c1, %c0;
; ABV-NEXT:     221:0|    3: <11, 1, 2, 1>         |    br i1 %v0, label %b1, label %b2;
; ABV-NEXT:          |                             |  %b1:
; ABV-NEXT:     225:2|    8: <10, 3>               |    ret i32 %c0; <@a4>
; ABV-NEXT:          |                             |  %b2:
; ABV-NEXT:     226:4|    5: <2, 4, 3, 1>          |    %v1 = sub i32 %p0, %c0; <@a1>
; ABV-NEXT:     229:0|    3: <34, 0, 6, 1>         |    %v2 = call i32 @f0(i32 %v1);
; ABV-NEXT:     234:0|    5: <2, 6, 1, 2>          |    %v3 = mul i32 %p0, %v2; <@a1>
; ABV-NEXT:     236:4|    8: <10, 1>               |    ret i32 %v3; <@a4>
; ABV-NEXT:     237:6|    1: <65535, 14, 3>        |    valuesymtab {  // BlockID = 14
; ABV-NEXT:     244:0|      7: <2, 1, 116, 114,    |      %b1 : "true"; <@a3>
; ABV-NEXT:          |        117, 101>            |
; ABV-NEXT:     249:1|      6: <1, 4, 118, 49>     |      %v0 : "v1"; <@a2>
; ABV-NEXT:     252:6|      6: <1, 5, 118, 50>     |      %v1 : "v2"; <@a2>
; ABV-NEXT:     256:3|      6: <1, 6, 118, 51>     |      %v2 : "v3"; <@a2>
; ABV-NEXT:     260:0|      6: <1, 7, 118, 52>     |      %v3 : "v4"; <@a2>
; ABV-NEXT:     263:5|      7: <2, 2, 102, 97, 108,|      %b2 : "false"; <@a3>
; ABV-NEXT:          |        115, 101>            |
; ABV-NEXT:     269:4|      6: <1, 1, 110>         |      %p0 : "n"; <@a2>
; ABV-NEXT:     272:3|    0: <65534>               |    }
; ABV-NEXT:     276:0|  0: <65534>                 |  }
; ABV-NEXT:     280:0|0: <65534>                   |}

; NOABV:            0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; NOABV-NEXT:          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; NOABV-NEXT:          | 0>                          |
; NOABV-NEXT:      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; NOABV-NEXT:      24:0|  3: <1, 1>                  |  version 1;
; NOABV-NEXT:      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; NOABV-NEXT:      36:0|  0: <65534>                 |  }
; NOABV-NEXT:      40:0|  1: <65535, 17, 2>          |  types {  // BlockID = 17
; NOABV-NEXT:      48:0|    3: <1, 4>                |    count 4;
; NOABV-NEXT:      50:4|    3: <7, 32>               |    @t0 = i32;
; NOABV-NEXT:      53:6|    3: <2>                   |    @t1 = void;
; NOABV-NEXT:      55:4|    3: <21, 0, 0, 0>         |    @t2 = i32 (i32);
; NOABV-NEXT:      59:4|    3: <7, 1>                |    @t3 = i1;
; NOABV-NEXT:      62:0|  0: <65534>                 |  }
; NOABV-NEXT:      64:0|  3: <8, 2, 0, 0, 0>         |  define external i32 @f0(i32);
; NOABV-NEXT:      68:6|  1: <65535, 19, 2>          |  globals {  // BlockID = 19
; NOABV-NEXT:      76:0|    3: <5, 0>                |    count 0;
; NOABV-NEXT:      78:4|  0: <65534>                 |  }
; NOABV-NEXT:      80:0|  1: <65535, 14, 2>          |  valuesymtab {  // BlockID = 14
; NOABV-NEXT:      88:0|    3: <1, 0, 102, 97, 99,   |    @f0 : "fact";
; NOABV-NEXT:          |        116>                 |
; NOABV-NEXT:      96:4|  0: <65534>                 |  }
; NOABV-NEXT:     100:0|  1: <65535, 12, 2>          |  function i32 @f0(i32 %p0) {  
; NOABV-NEXT:          |                             |                   // BlockID = 12
; NOABV-NEXT:     108:0|    3: <1, 3>                |    blocks 3;
; NOABV-NEXT:     110:4|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; NOABV-NEXT:     120:0|      3: <1, 0>              |      i32:
; NOABV-NEXT:     122:4|      3: <4, 2>              |        %c0 = i32 1;
; NOABV-NEXT:     125:0|      3: <4, 0>              |        %c1 = i32 0;
; NOABV-NEXT:     127:4|    0: <65534>               |      }
; NOABV-NEXT:          |                             |  %b0:
; NOABV-NEXT:     128:0|    3: <28, 1, 2, 32>        |    %v0 = icmp eq i32 %c1, %c0;
; NOABV-NEXT:     132:6|    3: <11, 1, 2, 1>         |    br i1 %v0, label %b1, label %b2;
; NOABV-NEXT:          |                             |  %b1:
; NOABV-NEXT:     136:6|    3: <10, 3>               |    ret i32 %c0;
; NOABV-NEXT:          |                             |  %b2:
; NOABV-NEXT:     139:2|    3: <2, 4, 3, 1>          |    %v1 = sub i32 %p0, %c0;
; NOABV-NEXT:     143:2|    3: <34, 0, 6, 1>         |    %v2 = call i32 @f0(i32 %v1);
; NOABV-NEXT:     148:0|    3: <2, 6, 1, 2>          |    %v3 = mul i32 %p0, %v2;
; NOABV-NEXT:     152:0|    3: <10, 1>               |    ret i32 %v3;
; NOABV-NEXT:     154:4|    1: <65535, 14, 2>        |    valuesymtab {  // BlockID = 14
; NOABV-NEXT:     164:0|      3: <2, 1, 116, 114,    |      %b1 : "true";
; NOABV-NEXT:          |        117, 101>            |
; NOABV-NEXT:     172:4|      3: <1, 4, 118, 49>     |      %v0 : "v1";
; NOABV-NEXT:     178:0|      3: <1, 5, 118, 50>     |      %v1 : "v2";
; NOABV-NEXT:     183:4|      3: <1, 6, 118, 51>     |      %v2 : "v3";
; NOABV-NEXT:     189:0|      3: <1, 7, 118, 52>     |      %v3 : "v4";
; NOABV-NEXT:     194:4|      3: <2, 2, 102, 97, 108,|      %b2 : "false";
; NOABV-NEXT:          |        115, 101>            |
; NOABV-NEXT:     204:4|      3: <1, 1, 110>         |      %p0 : "n";
; NOABV-NEXT:     208:4|    0: <65534>               |    }
; NOABV-NEXT:     212:0|  0: <65534>                 |  }
; NOABV-NEXT:     216:0|0: <65534>                   |}
