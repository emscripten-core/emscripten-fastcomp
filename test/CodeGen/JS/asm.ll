; RUN: not llc -march=js < %s

; Inline asm isn't supported (yet?). llc should report an error when it
; encounters inline asm.
;
; We could support the special case of an empty inline asm string without much
; work, but code that uses such things most likely isn't portable anyway, and
; there are usually much better alternatives.

define void @foo() {
  call void asm "", ""()
  ret void
}
