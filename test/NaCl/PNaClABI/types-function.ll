; RUN: opt -verify-pnaclabi-functions -analyze < %s | FileCheck %s
; Test type-checking in function bodies. This test is not intended to verify
; all the rules about the various types, but instead to make sure that types
; stashed in various places in function bodies are caught.

define void @types() {
; CHECK: Function types has instruction with disallowed type: half
  %h1 = fptrunc double undef to half
  ret void
}