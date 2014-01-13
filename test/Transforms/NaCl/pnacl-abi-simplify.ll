; RUN: opt %s -pnacl-abi-simplify-preopt -pnacl-abi-simplify-postopt -S \
; RUN:     | FileCheck %s

target datalayout = "p:32:32:32"

; Check that the "tail" attribute is preserved on calls.
define void @tail_call() {
  tail call void @tail_call()
  ret void
}
; CHECK: tail call void @tail_call()
