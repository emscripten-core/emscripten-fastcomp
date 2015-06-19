; Show that we can convert PNaCl bitcode into simplified record text
; (for fuzzing).

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcdis \
; RUN:              | FileCheck %s -check-prefix=BC

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcfuzz -convert-to-text -output - \
; RUN:              | FileCheck %s --check-prefix=TB

; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-bcfuzz -convert-to-text -output - \
; RUN:              | pnacl-thaw -bitcode-as-text -allow-local-symbol-tables \
; RUN:              | llvm-dis - \
; RUN:              | FileCheck %s --check-prefix=IN

define i32 @fact(i32 %p0) {
  %v0 = icmp ult i32 %p0, 1
  br i1 %v0, label %true, label %false
true:
  ret i32 1
false:
  %v2 = sub i32 %p0, 1
  %v3 = call i32 @fact(i32 %v2)
  %v4 = mul i32 %v3, %p0
  ret i32 %v4
}

; IN: define i32 @fact(i32 %p0) {
; IN:   %v0 = icmp ult i32 %p0, 1
; IN:   br i1 %v0, label %true, label %false
; IN: true:
; IN:   ret i32 1
; IN: false:
; IN:   %v2 = sub i32 %p0, 1
; IN:   %v3 = call i32 @fact(i32 %v2)
; IN:   %v4 = mul i32 %v3, %p0
; IN:   ret i32 %v4
; IN: }

; BC:        0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 88, 69)
; BC-NEXT:      | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2
; BC-NEXT:      | 0>                          |
; BC-NEXT:  16:0|1: <65535, 8, 2>             |module {  // BlockID = 8
; BC-NEXT:  24:0|  3: <1, 1>                  |  version 1;
; BC-NEXT:  26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID = 0
; BC-NEXT:  36:0|    3: <1, 14>               |    valuesymtab:
; BC-NEXT:  38:4|    2: <65533, 4, 0, 1, 3, 0,|      @a0 = abbrev <fixed(3), vbr(8), 
; BC-NEXT:      |        2, 8, 0, 3, 0, 1, 8> |                   array(fixed(8))>;
; BC-NEXT:  43:2|    2: <65533, 4, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8), 
; BC-NEXT:      |        8, 0, 3, 0, 1, 7>    |                   array(fixed(7))>;
; BC-NEXT:  48:0|    2: <65533, 4, 1, 1, 0, 2,|      @a2 = abbrev <1, vbr(8), 
; BC-NEXT:      |        8, 0, 3, 0, 4>       |                   array(char6)>;
; BC-NEXT:  52:1|    2: <65533, 4, 1, 2, 0, 2,|      @a3 = abbrev <2, vbr(8), 
; BC-NEXT:      |        8, 0, 3, 0, 4>       |                   array(char6)>;
; BC-NEXT:  56:2|    3: <1, 11>               |    constants:
; BC-NEXT:  58:6|    2: <65533, 2, 1, 1, 0, 1,|      @a0 = abbrev <1, fixed(3)>;
; BC-NEXT:      |        3>                   |
; BC-NEXT:  61:7|    2: <65533, 2, 1, 4, 0, 2,|      @a1 = abbrev <4, vbr(8)>;
; BC-NEXT:      |        8>                   |
; BC-NEXT:  65:0|    2: <65533, 2, 1, 4, 1, 0>|      @a2 = abbrev <4, 0>;
; BC-NEXT:  68:1|    2: <65533, 2, 1, 6, 0, 2,|      @a3 = abbrev <6, vbr(8)>;
; BC-NEXT:      |        8>                   |
; BC-NEXT:  71:2|    3: <1, 12>               |    function:
; BC-NEXT:  73:6|    2: <65533, 4, 1, 20, 0,  |      @a0 = abbrev <20, vbr(6), vbr(4),
; BC-NEXT:      |        2, 6, 0, 2, 4, 0, 2, |                   vbr(4)>;
; BC-NEXT:      |        4>                   |
; BC-NEXT:  79:1|    2: <65533, 4, 1, 2, 0, 2,|      @a1 = abbrev <2, vbr(6), vbr(6), 
; BC-NEXT:      |        6, 0, 2, 6, 0, 1, 4> |                   fixed(4)>;
; BC-NEXT:  84:4|    2: <65533, 4, 1, 3, 0, 2,|      @a2 = abbrev <3, vbr(6), 
; BC-NEXT:      |        6, 0, 1, 3, 0, 1, 4> |                   fixed(3), fixed(4)>;
; BC-NEXT:  89:7|    2: <65533, 1, 1, 10>     |      @a3 = abbrev <10>;
; BC-NEXT:  91:7|    2: <65533, 2, 1, 10, 0,  |      @a4 = abbrev <10, vbr(6)>;
; BC-NEXT:      |        2, 6>                |
; BC-NEXT:  95:0|    2: <65533, 1, 1, 15>     |      @a5 = abbrev <15>;
; BC-NEXT:  97:0|    2: <65533, 3, 1, 43, 0,  |      @a6 = abbrev <43, vbr(6), 
; BC-NEXT:      |        2, 6, 0, 1, 3>       |                   fixed(3)>;
; BC-NEXT: 101:2|    2: <65533, 4, 1, 24, 0,  |      @a7 = abbrev <24, vbr(6), vbr(6),
; BC-NEXT:      |        2, 6, 0, 2, 6, 0, 2, |                   vbr(4)>;
; BC-NEXT:      |        4>                   |
; BC-NEXT: 106:5|    3: <1, 19>               |    globals:
; BC-NEXT: 109:1|    2: <65533, 3, 1, 0, 0, 2,|      @a0 = abbrev <0, vbr(6), 
; BC-NEXT:      |        6, 0, 1, 1>          |                   fixed(1)>;
; BC-NEXT: 113:3|    2: <65533, 2, 1, 1, 0, 2,|      @a1 = abbrev <1, vbr(8)>;
; BC-NEXT:      |        8>                   |
; BC-NEXT: 116:4|    2: <65533, 2, 1, 2, 0, 2,|      @a2 = abbrev <2, vbr(8)>;
; BC-NEXT:      |        8>                   |
; BC-NEXT: 119:5|    2: <65533, 3, 1, 3, 0, 3,|      @a3 = abbrev <3, array(fixed(8))>
; BC-NEXT:      |        0, 1, 8>             |          ;
; BC-NEXT: 123:2|    2: <65533, 2, 1, 4, 0, 2,|      @a4 = abbrev <4, vbr(6)>;
; BC-NEXT:      |        6>                   |
; BC-NEXT: 126:3|    2: <65533, 3, 1, 4, 0, 2,|      @a5 = abbrev <4, vbr(6), vbr(6)>;
; BC-NEXT:      |        6, 0, 2, 6>          |
; BC-NEXT: 130:5|  0: <65534>                 |  }
; BC-NEXT: 132:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17
; BC-NEXT: 140:0|    2: <65533, 4, 1, 21, 0,  |    %a0 = abbrev <21, fixed(1), 
; BC-NEXT:      |        1, 1, 0, 3, 0, 1, 3> |                  array(fixed(3))>;
; BC-NEXT: 144:7|    3: <1, 4>                |    count 4;
; BC-NEXT: 147:4|    3: <7, 32>               |    @t0 = i32;
; BC-NEXT: 150:7|    3: <2>                   |    @t1 = void;
; BC-NEXT: 152:6|    4: <21, 0, 0, 0>         |    @t2 = i32 (i32); <%a0>
; BC-NEXT: 154:6|    3: <7, 1>                |    @t3 = i1;
; BC-NEXT: 157:3|  0: <65534>                 |  }
; BC-NEXT: 160:0|  3: <8, 2, 0, 0, 0>         |  define external i32 @f0(i32);
; BC-NEXT: 164:6|  1: <65535, 19, 4>          |  globals {  // BlockID = 19
; BC-NEXT: 172:0|    3: <5, 0>                |    count 0;
; BC-NEXT: 174:6|  0: <65534>                 |  }
; BC-NEXT: 176:0|  1: <65535, 14, 3>          |  valuesymtab {  // BlockID = 14
; BC-NEXT: 184:0|    6: <1, 0, 102, 97, 99,   |    @f0 : "fact"; <@a2>
; BC-NEXT:      |        116>                 |
; BC-NEXT: 189:1|  0: <65534>                 |  }
; BC-NEXT: 192:0|  1: <65535, 12, 4>          |  function i32 @f0(i32 %p0) {  
; BC-NEXT:      |                             |                   // BlockID = 12
; BC-NEXT: 200:0|    3: <1, 3>                |    blocks 3;
; BC-NEXT: 202:6|    1: <65535, 11, 3>        |    constants {  // BlockID = 11
; BC-NEXT: 212:0|      4: <1, 0>              |      i32: <@a0>
; BC-NEXT: 212:6|      5: <4, 2>              |        %c0 = i32 1; <@a1>
; BC-NEXT: 214:1|    0: <65534>               |      }
; BC-NEXT:      |                             |  %b0:
; BC-NEXT: 216:0|    3: <28, 2, 1, 36>        |    %v0 = icmp ult i32 %p0, %c0;
; BC-NEXT: 221:0|    3: <11, 1, 2, 1>         |    br i1 %v0, label %b1, label %b2;
; BC-NEXT:      |                             |  %b1:
; BC-NEXT: 225:2|    8: <10, 2>               |    ret i32 %c0; <@a4>
; BC-NEXT:      |                             |  %b2:
; BC-NEXT: 226:4|    5: <2, 3, 2, 1>          |    %v1 = sub i32 %p0, %c0; <@a1>
; BC-NEXT: 229:0|    3: <34, 0, 5, 1>         |    %v2 = call i32 @f0(i32 %v1);
; BC-NEXT: 234:0|    5: <2, 1, 5, 2>          |    %v3 = mul i32 %v2, %p0; <@a1>
; BC-NEXT: 236:4|    8: <10, 1>               |    ret i32 %v3; <@a4>
; BC-NEXT: 237:6|  0: <65534>                 |  }
; BC-NEXT: 240:0|0: <65534>                   |}

; TB:      65535,8,2;
; TB-NEXT: 1,1;
; NOTE: The blockinfo block has been removed.
; TB-NEXT: 65535,17,2;
; NOTE: The local type abbreviation has been removed.
; TB-NEXT: 1,4;
; TB-NEXT: 7,32;
; TB-NEXT: 2;
; TB-NEXT: 21,0,0,0;
; TB-NEXT: 7,1;
; TB-NEXT: 65534;
; TB-NEXT: 8,2,0,0,0;
; TB-NEXT: 65535,19,2;
; TB-NEXT: 5,0;
; TB-NEXT: 65534;
; TB-NEXT: 65535,14,2;
; TB-NEXT: 1,0,102,97,99,116;
; TB-NEXT: 65534;
; TB-NEXT: 65535,12,2;
; TB-NEXT: 1,3;
; TB-NEXT: 65535,11,2;
; TB-NEXT: 1,0;
; TB-NEXT: 4,2;
; TB-NEXT: 65534;
; TB-NEXT: 28,2,1,36;
; TB-NEXT: 11,1,2,1;
; TB-NEXT: 10,2;
; TB-NEXT: 2,3,2,1;
; TB-NEXT: 34,0,5,1;
; TB-NEXT: 2,1,5,2;
; TB-NEXT: 10,1;
; TB-NEXT: 65534;
; TB-NEXT: 65534;
