; Test how we handle eliding inttoptr instructions.

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-thaw -allow-local-symbol-tables \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD2

; ------------------------------------------------------

; Test that we elide the simple case of inttoptr of a load.
define void @SimpleLoad(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  ret void
}

; TD2:      define void @SimpleLoad(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_RET/>
; PF2-NEXT:  </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test that we can handle multiple inttoptr of loads.
define i32 @TwoLoads(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  %3 = inttoptr i32 %i to i32*
  %4 = load i32* %3, align 4
  %5 = add i32 %2, %4
  ret i32 %5
}

; TD2:      define i32 @TwoLoads(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   %3 = load i32* %1, align 4
; TD2-NEXT:   %4 = add i32 %2, %3
; TD2-NEXT:   ret i32 %4
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_LOAD op0=2 op1=3 op2=0/>
; PF2-NEXT:    <INST_BINOP op0=2 op1=1 op2=0/>
; PF2-NEXT:    <INST_RET op0=1/>
; PF2-NEXT:  </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test how we handle inttoptrs, if optimized in the input file. This
; case tests within a single block.
define i32 @TwoLoadOptOneBlock(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  %3 = load i32* %1, align 4
  %4 = add i32 %2, %3
  ret i32 %4
}

; TD2:      define i32 @TwoLoadOptOneBlock(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   %3 = load i32* %1, align 4
; TD2-NEXT:   %4 = add i32 %2, %3
; TD2-NEXT:   ret i32 %4
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_LOAD op0=2 op1=3 op2=0/>
; PF2-NEXT:    <INST_BINOP op0=2 op1=1 op2=0/>
; PF2-NEXT:    <INST_RET op0=1/>
; PF2-NEXT:  </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test how we handle inttoptrs if optimized in the input file.  This
; case tests accross blocks.
define i32 @TwoLoadOptTwoBlocks(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  %3 = load i32* %1, align 4
  %4 = add i32 %2, %3
  br label %BB

BB:
  %5 = load i32* %1, align 4
  %6 = load i32* %1, align 4
  %7 = add i32 %5, %6
  ret i32 %7
}

; TD2:      define i32 @TwoLoadOptTwoBlocks(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   %3 = load i32* %1, align 4
; TD2-NEXT:   %4 = add i32 %2, %3
; TD2-NEXT:   br label %BB
; TD2:      BB:
; TD2-NEXT:   %5 = inttoptr i32 %i to i32*
; TD2-NEXT:   %6 = load i32* %5, align 4
; TD2-NEXT:   %7 = load i32* %5, align 4
; TD2-NEXT:   %8 = add i32 %6, %7
; TD2-NEXT:   ret i32 %8
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2-NEXT:     <DECLAREBLOCKS op0=2/>
; PF2-NEXT:     <INST_LOAD op0=1 op1=3 op2=0/>
; PF2-NEXT:     <INST_LOAD op0=2 op1=3 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=2 op1=1 op2=0/>
; PF2-NEXT:     <INST_BR op0=1/>
; PF2-NEXT:     <INST_LOAD op0=4 op1=3 op2=0/>
; PF2-NEXT:     <INST_LOAD op0=5 op1=3 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=2 op1=1 op2=0/>
; PF2-NEXT:     <INST_RET op0=1/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test that we elide the simple case of inttoptr for a store.
define void @SimpleStore(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  store i32 %i, i32* %1, align 4
  ret void
}

; TD2:      define void @SimpleStore(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   store i32 %i, i32* %1, align 4
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_STORE op0=1 op1=1 op2=3/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>
