; RUN: llc < %s | FileCheck %s

; Test simple getelementptr codegen.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; Test that trailing indices are folded.

; CHECK: function _getelementptr([[VAL_P:\$[a-z_]+]]) {
; CHECK:  [[GEP:\$[a-z_]+]] = ((([[GEPINT:\$[a-z_]+]])) + 588|0);
define i32* @getelementptr([10 x [12 x i32] ]* %p) {
  %t = getelementptr [10 x [12 x i32]], [10 x [12 x i32]]* %p, i32 1, i32 2, i32 3
  ret i32* %t
}

%struct.A = type { i32, [34 x i16] }

@global = global [72 x i8] zeroinitializer, align 4

; Fold globals into getelementptr addressing.

; CHECK: function _fold_global($i) {
; CHECK: $add = (($i) + 34)|0;
; CHECK: $arrayidx = (12 + ($add<<1)|0);
; CHECK: $t0 = HEAP16[$arrayidx>>1]|0;
define i16 @fold_global(i32 %i) {
  %add = add i32 %i, 34
  %arrayidx = getelementptr %struct.A, %struct.A* bitcast ([72 x i8]* @global to %struct.A*), i32 0, i32 1, i32 %add
  %t0 = load volatile i16, i16* %arrayidx, align 2
  ret i16 %t0
}

; Don't reassociate the indices of a getelementptr, which would increase
; the chances of creating out-of-bounds intermediate values.

; CHECK: function _no_reassociate($p,$i) {
; CHECK: $add = (($i) + 34)|0;
; CHECK: $arrayidx = (((($p)) + 4|0) + ($add<<1)|0);
; CHECK: $t0 = HEAP16[$arrayidx>>1]|0;
define i16 @no_reassociate(%struct.A* %p, i32 %i) {
  %add = add i32 %i, 34
  %arrayidx = getelementptr %struct.A, %struct.A* %p, i32 0, i32 1, i32 %add
  %t0 = load volatile i16, i16* %arrayidx, align 2
  ret i16 %t0
}

