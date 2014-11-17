; RUN: not llc < %s

; Inline asm isn't supported (yet?). llc should report an error when it
; encounters inline asm.
;
; We could support the special case of an empty inline asm string without much
; work, but code that uses such things most likely isn't portable anyway, and
; there are usually much better alternatives.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

define void @foo() {
  call void asm "", ""()
  ret void
}
