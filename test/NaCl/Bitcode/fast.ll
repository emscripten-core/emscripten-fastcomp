; Tests that we don't write the fast (floating point) attributes into
; PNaCl bitcode files (i.e. flags fast, nnan, ninf, nsz, and arcp).

; Test 1: Show that flags are removed.
; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - \
; RUN:              | FileCheck %s

; Test 2: Show that the bitcode files do not contain flags (i.e.
; the corresponding BINOP records only have 3 values, not 4).
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=RECORD

define void @foo() {
  ; Show that we handle all flags for fadd
  %1 = fadd fast double 1.000000e+00, 2.000000e+00
  %2 = fadd nnan double 3.000000e+00, 4.000000e+00
  %3 = fadd ninf double 5.000000e+00, 6.000000e+00
  %4 = fadd nsz double 7.000000e+00, 8.000000e+00
  %5 = fadd arcp double 9.000000e+00, 10.000000e+00

; CHECK:   %1 = fadd double 1.000000e+00, 2.000000e+00
; CHECK:   %2 = fadd double 3.000000e+00, 4.000000e+00
; CHECK:   %3 = fadd double 5.000000e+00, 6.000000e+00
; CHECK:   %4 = fadd double 7.000000e+00, 8.000000e+00
; CHECK:   %5 = fadd double 9.000000e+00, 1.000000e+01

; RECORD: <INST_BINOP op0=10 op1=9 op2=0/>
; RECORD: <INST_BINOP op0=9 op1=8 op2=0/>
; RECORD: <INST_BINOP op0=8 op1=7 op2=0/>
; RECORD: <INST_BINOP op0=7 op1=6 op2=0/>
; RECORD: <INST_BINOP op0=6 op1=5 op2=0/>

  ; Show that we handle all flags for fsub
  %6 = fsub fast double 1.000000e+00, 2.000000e+00
  %7 = fsub nnan double 3.000000e+00, 4.000000e+00
  %8 = fsub ninf double 5.000000e+00, 6.000000e+00
  %9 = fsub nsz double 7.000000e+00, 8.000000e+00
  %10 = fsub arcp double 9.000000e+00, 10.000000e+00

; CHECK:   %6 = fsub double 1.000000e+00, 2.000000e+00
; CHECK:   %7 = fsub double 3.000000e+00, 4.000000e+00
; CHECK:   %8 = fsub double 5.000000e+00, 6.000000e+00
; CHECK:   %9 = fsub double 7.000000e+00, 8.000000e+00
; CHECK:   %10 = fsub double 9.000000e+00, 1.000000e+01

; RECORD: <INST_BINOP op0=15 op1=14 op2=1/>
; RECORD: <INST_BINOP op0=14 op1=13 op2=1/>
; RECORD: <INST_BINOP op0=13 op1=12 op2=1/>
; RECORD: <INST_BINOP op0=12 op1=11 op2=1/>
; RECORD: <INST_BINOP op0=11 op1=10 op2=1/>

  ; Show that we can handle all flags for fmul
  %11 = fmul fast double 1.000000e+00, 2.000000e+00
  %12 = fmul nnan double 3.000000e+00, 4.000000e+00
  %13 = fmul ninf double 5.000000e+00, 6.000000e+00
  %14 = fmul nsz double 7.000000e+00, 8.000000e+00
  %15 = fmul arcp double 9.000000e+00, 10.000000e+00

; CHECK:   %11 = fmul double 1.000000e+00, 2.000000e+00
; CHECK:   %12 = fmul double 3.000000e+00, 4.000000e+00
; CHECK:   %13 = fmul double 5.000000e+00, 6.000000e+00
; CHECK:   %14 = fmul double 7.000000e+00, 8.000000e+00
; CHECK:   %15 = fmul double 9.000000e+00, 1.000000e+01

; RECORD: <INST_BINOP op0=20 op1=19 op2=2/>
; RECORD: <INST_BINOP op0=19 op1=18 op2=2/>
; RECORD: <INST_BINOP op0=18 op1=17 op2=2/>
; RECORD: <INST_BINOP op0=17 op1=16 op2=2/>
; RECORD: <INST_BINOP op0=16 op1=15 op2=2/>

  ; Show that we can handle all flags for fdiv
  %16 = fdiv fast double 1.000000e+00, 2.000000e+00
  %17 = fdiv nnan double 3.000000e+00, 4.000000e+00
  %18 = fdiv ninf double 5.000000e+00, 6.000000e+00
  %19 = fdiv nsz double 7.000000e+00, 8.000000e+00
  %20 = fdiv arcp double 9.000000e+00, 10.000000e+00

; CHECK:   %16 = fdiv double 1.000000e+00, 2.000000e+00
; CHECK:   %17 = fdiv double 3.000000e+00, 4.000000e+00
; CHECK:   %18 = fdiv double 5.000000e+00, 6.000000e+00
; CHECK:   %19 = fdiv double 7.000000e+00, 8.000000e+00
; CHECK:   %20 = fdiv double 9.000000e+00, 1.000000e+01

; RECORD: <INST_BINOP op0=25 op1=24 op2=4/>
; RECORD: <INST_BINOP op0=24 op1=23 op2=4/>
; RECORD: <INST_BINOP op0=23 op1=22 op2=4/>
; RECORD: <INST_BINOP op0=22 op1=21 op2=4/>
; RECORD: <INST_BINOP op0=21 op1=20 op2=4/>

  ; Show that we can handle all flags for frem.
  %21 = frem fast double 1.000000e+00, 2.000000e+00
  %22 = frem nnan double 3.000000e+00, 4.000000e+00
  %23 = frem ninf double 5.000000e+00, 6.000000e+00
  %24 = frem nsz double 7.000000e+00, 8.000000e+00
  %25 = frem arcp double 9.000000e+00, 10.000000e+00

; CHECK:   %21 = frem double 1.000000e+00, 2.000000e+00
; CHECK:   %22 = frem double 3.000000e+00, 4.000000e+00
; CHECK:   %23 = frem double 5.000000e+00, 6.000000e+00
; CHECK:   %24 = frem double 7.000000e+00, 8.000000e+00
; CHECK:   %25 = frem double 9.000000e+00, 1.000000e+01

; RECORD: <INST_BINOP op0=30 op1=29 op2=6/>
; RECORD: <INST_BINOP op0=29 op1=28 op2=6/>
; RECORD: <INST_BINOP op0=28 op1=27 op2=6/>
; RECORD: <INST_BINOP op0=27 op1=26 op2=6/>
; RECORD: <INST_BINOP op0=26 op1=25 op2=6/>

  ret void
}
