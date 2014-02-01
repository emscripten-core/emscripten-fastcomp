; RUN: opt -S < %s -expand-getelementptr | llc -march=js | FileCheck %s

; Test simple getelementptr codegen.

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128"

; CHECK: function _getelementptr([[VAL_P:\$[a-z_]+]]) {
; CHECK:  [[GEP:\$[a-z_]+]] = (([[GEPINT:\$[a-z_]+]]) + 588)|0;
define i32* @getelementptr([10 x [12 x i32] ]* %p) {
  %t = getelementptr [10 x [12 x i32]]* %p, i32 1, i32 2, i32 3
  ret i32* %t
}
