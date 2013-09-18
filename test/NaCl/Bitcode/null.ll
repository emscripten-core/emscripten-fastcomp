; Test that we no longer generate NULL for numeric constants.

; RUN: llvm-as < %s | pnacl-freeze  \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD

; ------------------------------------------------------

define void @TestIntegers() {
  %1 = and i1 true, false
  %2 = add i8 1, 0
  %3 = add i16 1, 0
  %4 = add i32 1, 0
  %5 = add i64 1, 0
  ret void
}

; TD:      define void @TestIntegers() {
; TD-NEXT:   %1 = and i1 true, false
; TD-NEXT:   %2 = add i8 1, 0
; TD-NEXT:   %3 = add i16 1, 0
; TD-NEXT:   %4 = add i32 1, 0
; TD-NEXT:   %5 = add i64 1, 0
; TD-NEXT:   ret void
; TD-NEXT: }

; PF:      <FUNCTION_BLOCK>
; PF-NEXT:   <DECLAREBLOCKS op0=1/>
; PF-NEXT:   <CONSTANTS_BLOCK>
; PF-NEXT:     <SETTYPE op0=1/>
; PF-NEXT:     <INTEGER op0=3/>
; PF-NEXT:     <INTEGER op0=0/>
; PF-NEXT:     <SETTYPE op0=2/>
; PF-NEXT:     <INTEGER op0=2/>
; PF-NEXT:     <INTEGER op0=0/>
; PF-NEXT:     <SETTYPE op0=3/>
; PF-NEXT:     <INTEGER op0=2/>
; PF-NEXT:     <INTEGER op0=0/>
; PF-NEXT:     <SETTYPE op0=4/>
; PF-NEXT:     <INTEGER op0=2/>
; PF-NEXT:     <INTEGER op0=0/>
; PF-NEXT:     <SETTYPE op0=5/>
; PF-NEXT:     <INTEGER op0=2/>
; PF-NEXT:     <INTEGER op0=0/>
; PF-NEXT:   </CONSTANTS_BLOCK>
; PF-NEXT:   <INST_BINOP op0=10 op1=9 op2=10/>
; PF-NEXT:   <INST_BINOP op0=9 op1=8 op2=0/>
; PF-NEXT:   <INST_BINOP op0=8 op1=7 op2=0/>
; PF-NEXT:   <INST_BINOP op0=7 op1=6 op2=0/>
; PF-NEXT:   <INST_BINOP op0=6 op1=5 op2=0/>
; PF-NEXT:   <INST_RET/>
; PF-NEXT: </FUNCTION_BLOCK>

define void @TestFloats() {
  %1 = fadd float 1.0, 0.0
  %2 = fadd double 1.0, 0.0
  ret void
}

; TD:      define void @TestFloats() {
; TD-NEXT:   %1 = fadd float 1.000000e+00, 0.000000e+00
; TD-NEXT:   %2 = fadd double 1.000000e+00, 0.000000e+00
; TD-NEXT:   ret void
; TD-NEXT: }

; PF:      <FUNCTION_BLOCK>
; PF-NEXT:   <DECLAREBLOCKS op0=1/>
; PF-NEXT:   <CONSTANTS_BLOCK>
; PF-NEXT:     <SETTYPE op0=6/>
; PF-NEXT:     <FLOAT op0=1065353216/>
; PF-NEXT:     <FLOAT op0=0/>
; PF-NEXT:     <SETTYPE op0=7/>
; PF-NEXT:     <FLOAT op0=4607182418800017408/>
; PF-NEXT:     <FLOAT op0=0/>
; PF-NEXT:   </CONSTANTS_BLOCK>
; PF-NEXT:   <INST_BINOP op0=4 op1=3 op2=0/>
; PF-NEXT:   <INST_BINOP op0=3 op1=2 op2=0/>
; PF-NEXT:   <INST_RET/>
; PF-NEXT: </FUNCTION_BLOCK>
