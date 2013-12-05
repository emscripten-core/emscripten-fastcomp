; Tests that we don't write the fast (floating point) attributes into
; PNaCl bitcode files (i.e. flags fast, nnan, ninf, nsz, and arcp).

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - \
; RUN:              | FileCheck %s

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

  ret void
}
