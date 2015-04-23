; RUN: llc < %s | FileCheck %s

; Use proper types to ffi calls, no float32

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK:      (+Math_sqrt(+1));
; CHECK-NEXT: (+Math_sqrt(+1));
; CHECK-NEXT: (+Math_sqrt((+$d)));
; CHECK-NEXT: (+Math_sqrt((+$f)));
; CHECK-NEXT: (+Math_ceil(+1));
; CHECK-NEXT: (+Math_ceil(+1));
; CHECK-NEXT: (+Math_floor(+1));
; CHECK-NEXT: (+Math_floor(+1));
; CHECK-NEXT: (+_min(+1,+1));
; CHECK-NEXT: (+_fmin(+1,+1));
; CHECK-NEXT: (+_max(+1,+1));
; CHECK-NEXT: (+_fmax(+1,+1));
; CHECK-NEXT: (+Math_abs(+1));
; CHECK-NEXT: (+_absf(+1));
; CHECK-NEXT: (+Math_sin(+1));
; CHECK-NEXT: (+Math_sin(+1));
define void @foo(i32 %x) {
entry:
  %f = fadd float 1.0, 2.0
  %d = fadd double 1.0, 2.0

  %sqrtd = call double @sqrt(double 1.0)
  %sqrtf = call float @sqrtf(float 1.0)
  %sqrtdv = call double @sqrt(double %d) ; check vars too
  %sqrtfv = call float @sqrtf(float %f)

  %ceild = call double @ceil(double 1.0)
  %ceilf = call float @ceilf(float 1.0)

  %floord = call double @floor(double 1.0)
  %floorf = call float @floorf(float 1.0)

  ; these could be optimized in theory

  %mind = call double @min(double 1.0, double 1.0)
  %minf = call float @fmin(float 1.0, float 1.0)

  %maxd = call double @max(double 1.0, double 1.0)
  %maxf = call float @fmax(float 1.0, float 1.0)

  %absd = call double @abs(double 1.0)
  %absf = call float @absf(float 1.0)

  ; sin is NOT optimizable with floats

  %sind = call double @sin(double 1.0)
  %sinf = call float @sinf(float 1.0)

  ret void
}

declare double @sqrt(double %x)
declare float @sqrtf(float %x)

declare double @ceil(double %x)
declare float @ceilf(float %x)

declare double @floor(double %x)
declare float @floorf(float %x)

declare double @min(double %x, double %y)
declare float @fmin(float %x, float %y)

declare double @max(double %x, double %y)
declare float @fmax(float %x, float %y)

declare double @abs(double %x)
declare float @absf(float %x)

declare double @sin(double %x)
declare float @sinf(float %x)

attributes #0 = { nounwind readnone }

