; RUN: not pnacl-abicheck < %s | FileCheck %s

; Disallow the visibility attributes set by
; __attribute__((visibility("hidden"))) and
; __attribute__((visibility("protected"))).

define hidden void @visibility_hidden() {
  ret void
}
; CHECK: Function visibility_hidden has disallowed visibility: hidden

define protected void @visibility_protected() {
  ret void
}
; CHECK: Function visibility_protected has disallowed visibility: protected
