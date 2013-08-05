; Test how we handle eliding inttoptr instructions.
; TODO(kschimpf) Expand these tests as further CL's are added for issue 3544.

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=1 | pnacl-bcanalyzer -dump \
; RUN:              | FileCheck %s -check-prefix=PF1

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=1 | pnacl-thaw \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD1

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=2 | pnacl-bcanalyzer -dump \
; RUN:              | FileCheck %s -check-prefix=PF2

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=2 | pnacl-thaw \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD2

; ------------------------------------------------------

; Test that we elide the simple case of inttoptr of a load.
define void @SimpleLoad(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  ret void
}

; TD1:      define void @SimpleLoad(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to i32*
; TD1-NEXT:   %2 = load i32* %1, align 4
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:       <FUNCTION_BLOCK NumWords=6 BlockCodeSize=4>
; PF1-NEXT:    <DECLAREBLOCKS op0=1/>
; PF1-NEXT:    <INST_CAST abbrevid=7 op0=1 op1=1 op2=10/>
; PF1-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF1-NEXT:    <INST_RET abbrevid=8/>
; PF1:       </FUNCTION_BLOCK>

; TD2:      define void @SimpleLoad(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK NumWords=5 BlockCodeSize=4>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_RET abbrevid=8/>
; PF2:       </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test that we don't elide an inttoptr if one of its uses is not a load.
define i32* @NonsimpleLoad(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  ret i32* %1
}

; TD1:      define i32* @NonsimpleLoad(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to i32*
; TD1-NEXT:   %2 = load i32* %1, align 4
; TD1-NEXT:   ret i32* %1
; TD1-NEXT: }

; PF1:       <FUNCTION_BLOCK NumWords=6 BlockCodeSize=4>
; PF1-NEXT:    <DECLAREBLOCKS op0=1/>
; PF1-NEXT:    <INST_CAST abbrevid=7 op0=1 op1=1 op2=10/>
; PF1-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF1-NEXT:    <INST_RET abbrevid=9 op0=2/>
; PF1:       </FUNCTION_BLOCK>

; TD2:      define i32* @NonsimpleLoad(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   ret i32* %1
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK NumWords=6 BlockCodeSize=4>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_CAST abbrevid=7 op0=1 op1=1 op2=10/>
; PF2-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_RET abbrevid=9 op0=2/>
; PF2:       </FUNCTION_BLOCK>

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

; TD1:      define i32 @TwoLoads(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to i32*
; TD1-NEXT:   %2 = load i32* %1, align 4
; TD1-NEXT:   %3 = inttoptr i32 %i to i32*
; TD1-NEXT:   %4 = load i32* %3, align 4
; TD1-NEXT:   %5 = add i32 %2, %4
; TD1-NEXT:   ret i32 %5
; TD1-NEXT: }

; PF1:       <FUNCTION_BLOCK NumWords=8 BlockCodeSize=4>
; PF1-NEXT:    <DECLAREBLOCKS op0=1/>
; PF1-NEXT:    <INST_CAST abbrevid=7 op0=1 op1=1 op2=10/>
; PF1-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF1-NEXT:    <INST_CAST abbrevid=7 op0=3 op1=1 op2=10/>
; PF1-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF1-NEXT:    <INST_BINOP abbrevid=5 op0=3 op1=1 op2=0/>
; PF1-NEXT:    <INST_RET abbrevid=9 op0=1/>
; PF1:       </FUNCTION_BLOCK>

; TD2:      define i32 @TwoLoads(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   %3 = inttoptr i32 %i to i32*
; TD2-NEXT:   %4 = load i32* %3, align 4
; TD2-NEXT:   %5 = add i32 %2, %4
; TD2-NEXT:   ret i32 %5
; TD2-NEXT: }


; PF2:       <FUNCTION_BLOCK NumWords=7 BlockCodeSize=4>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_LOAD abbrevid=4 op0=2 op1=3 op2=0/>
; PF2-NEXT:    <INST_BINOP abbrevid=5 op0=2 op1=1 op2=0/>
; PF2-NEXT:    <INST_RET abbrevid=9 op0=1/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test how we duplicate inttoptrs, even if optimized in the input file.
define i32 @TwoLoadOpt(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = load i32* %1, align 4
  %3 = load i32* %1, align 4
  %4 = add i32 %2, %3
  ret i32 %4
}

; TD1:      define i32 @TwoLoadOpt(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to i32*
; TD1-NEXT:   %2 = load i32* %1, align 4
; TD1-NEXT:   %3 = load i32* %1, align 4
; TD1-NEXT:   %4 = add i32 %2, %3
; TD1-NEXT:   ret i32 %4
; TD1-NEXT: }

; PF1:       <FUNCTION_BLOCK NumWords=7 BlockCodeSize=4>
; PF1-NEXT:    <DECLAREBLOCKS op0=1/>
; PF1-NEXT:    <INST_CAST abbrevid=7 op0=1 op1=1 op2=10/>
; PF1-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF1-NEXT:    <INST_LOAD abbrevid=4 op0=2 op1=3 op2=0/>
; PF1-NEXT:    <INST_BINOP abbrevid=5 op0=2 op1=1 op2=0/>
; PF1-NEXT:    <INST_RET abbrevid=9 op0=1/>
; PF1:       </FUNCTION_BLOCK>

; TD2:      define i32 @TwoLoadOpt(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = load i32* %1, align 4
; TD2-NEXT:   %3 = inttoptr i32 %i to i32*
; TD2-NEXT:   %4 = load i32* %3, align 4
; TD2-NEXT:   %5 = add i32 %2, %4
; TD2-NEXT:   ret i32 %5
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK NumWords=7 BlockCodeSize=4>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD abbrevid=4 op0=1 op1=3 op2=0/>
; PF2-NEXT:    <INST_LOAD abbrevid=4 op0=2 op1=3 op2=0/>
; PF2-NEXT:    <INST_BINOP abbrevid=5 op0=2 op1=1 op2=0/>
; PF2-NEXT:    <INST_RET abbrevid=9 op0=1/>
; PF2:       </FUNCTION_BLOCK>

; ------------------------------------------------------

; Test that we elide the simple case of inttoptr for a store.
define void @SimpleStore(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  store i32 %i, i32* %1, align 4
  ret void
}

; TD1:      define void @SimpleStore(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to i32*
; TD1-NEXT:   store i32 %i, i32* %1, align 4
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK NumWords=6 BlockCodeSize=4>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CAST abbrevid=7 op0=1 op1=1 op2=10/>
; PF1-NEXT:   <INST_STORE abbrevid=12 op0=1 op1=2 op2=3 op3=0/>
; PF1-NEXT:   <INST_RET abbrevid=8/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @SimpleStore(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   store i32 %i, i32* %1, align 4
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK NumWords=5 BlockCodeSize=4>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_STORE abbrevid=12 op0=1 op1=1 op2=3/>
; PF2-NEXT:   <INST_RET abbrevid=8/>
; PF2T:     </FUNCTION_BLOCK>
