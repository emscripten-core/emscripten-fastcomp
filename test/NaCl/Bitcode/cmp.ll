; Tests that all comparison conditions survive through PNaCl bitcode files.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - \
; RUN:              | FileCheck %s

define void @IntCompare() {
  %1 = icmp eq i32 0, 1
  %2 = icmp ne i32 0, 1
  %3 = icmp ugt i32 0, 1
  %4 = icmp uge i32 0, 1
  %5 = icmp ult i32 0, 1
  %6 = icmp ule i32 0, 1
  %7 = icmp sgt i32 0, 1
  %8 = icmp sge i32 0, 1
  %9 = icmp slt i32 0, 1
  %10 = icmp sle i32 0, 1
  ret void
}

; CHECK: define void @IntCompare() {
; CHECK:   %1 = icmp eq i32 0, 1
; CHECK:   %2 = icmp ne i32 0, 1
; CHECK:   %3 = icmp ugt i32 0, 1
; CHECK:   %4 = icmp uge i32 0, 1
; CHECK:   %5 = icmp ult i32 0, 1
; CHECK:   %6 = icmp ule i32 0, 1
; CHECK:   %7 = icmp sgt i32 0, 1
; CHECK:   %8 = icmp sge i32 0, 1
; CHECK:   %9 = icmp slt i32 0, 1
; CHECK:   %10 = icmp sle i32 0, 1
; CHECK:   ret void
; CHECK: }

define void @FloatCompare() {
  %1 = fcmp false float 0.000000e+00, 1.000000e+00
  %2 = fcmp oeq float 0.000000e+00, 1.000000e+00
  %3 = fcmp ogt float 0.000000e+00, 1.000000e+00
  %4 = fcmp oge float 0.000000e+00, 1.000000e+00
  %5 = fcmp olt float 0.000000e+00, 1.000000e+00
  %6 = fcmp ole float 0.000000e+00, 1.000000e+00
  %7 = fcmp one float 0.000000e+00, 1.000000e+00
  %8 = fcmp ord float 0.000000e+00, 1.000000e+00
  %9 = fcmp ueq float 0.000000e+00, 1.000000e+00
  %10 = fcmp ugt float 0.000000e+00, 1.000000e+00
  %11 = fcmp uge float 0.000000e+00, 1.000000e+00
  %12 = fcmp ult float 0.000000e+00, 1.000000e+00
  %13 = fcmp ule float 0.000000e+00, 1.000000e+00
  %14 = fcmp une float 0.000000e+00, 1.000000e+00
  %15 = fcmp uno float 0.000000e+00, 1.000000e+00
  %16 = fcmp true float 0.000000e+00, 1.000000e+00
  ret void
}

; CHECK: define void @FloatCompare() {
; CHECK:   %1 = fcmp false float 0.000000e+00, 1.000000e+00
; CHECK:   %2 = fcmp oeq float 0.000000e+00, 1.000000e+00
; CHECK:   %3 = fcmp ogt float 0.000000e+00, 1.000000e+00
; CHECK:   %4 = fcmp oge float 0.000000e+00, 1.000000e+00
; CHECK:   %5 = fcmp olt float 0.000000e+00, 1.000000e+00
; CHECK:   %6 = fcmp ole float 0.000000e+00, 1.000000e+00
; CHECK:   %7 = fcmp one float 0.000000e+00, 1.000000e+00
; CHECK:   %8 = fcmp ord float 0.000000e+00, 1.000000e+00
; CHECK:   %9 = fcmp ueq float 0.000000e+00, 1.000000e+00
; CHECK:   %10 = fcmp ugt float 0.000000e+00, 1.000000e+00
; CHECK:   %11 = fcmp uge float 0.000000e+00, 1.000000e+00
; CHECK:   %12 = fcmp ult float 0.000000e+00, 1.000000e+00
; CHECK:   %13 = fcmp ule float 0.000000e+00, 1.000000e+00
; CHECK:   %14 = fcmp une float 0.000000e+00, 1.000000e+00
; CHECK:   %15 = fcmp uno float 0.000000e+00, 1.000000e+00
; CHECK:   %16 = fcmp true float 0.000000e+00, 1.000000e+00
; CHECK:   ret void
; CHECK: }
