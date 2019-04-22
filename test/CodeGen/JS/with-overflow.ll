; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

declare { i32, i1 } @llvm.uadd.with.overflow.i32(i32, i32)

; CHECK-LABEL: function _test($x)
; CHECK-NEXT: $x = $x|0;
; CHECK-NEXT: var $a$arith = 0, label = 0, sp = 0;
; CHECK-NEXT: sp = STACKTOP;
; CHECK-NEXT: $a$arith = (($x) + 1)|0;
; CHECK-NEXT: return ($a$arith|0);
define i32 @test(i32 %x) {
  %a = call { i32, i1 } @llvm.uadd.with.overflow.i32(i32 %x, i32 1)
  %b = insertvalue { i32, i1 } %a, i1 false, 1
  %c = extractvalue { i32, i1 } %b, 0
  ret i32 %c
}
