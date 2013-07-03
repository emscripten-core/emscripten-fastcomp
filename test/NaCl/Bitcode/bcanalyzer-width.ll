; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump \
; RUN:              | FileCheck %s -check-prefix=BC
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump -operands-per-line=2 \
; RUN:              | FileCheck %s -check-prefix=BC2
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump -operands-per-line=8 \
; RUN:              | FileCheck %s -check-prefix=BC8

; Test that the command-line option -operands-per-line works as expected.

@bytes = internal global [10 x i8] c"abcdefghij"

; BC: <DATA abbrevid=7 op0=97 op1=98 op2=99 op3=100 op4=101 op5=102 op6=103 op7=104 op8=105 op9=106/>

; BC2: <DATA abbrevid=7 op0=97 op1=98
; BC2:       op2=99 op3=100
; BC2:       op4=101 op5=102
; BC2:       op6=103 op7=104
; BC2:       op8=105 op9=106/>

; BC8: <DATA abbrevid=7 op0=97 op1=98 op2=99 op3=100 op4=101 op5=102 op6=103 op7=104
; BC8:       op8=105 op9=106/>
