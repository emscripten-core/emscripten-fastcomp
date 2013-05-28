; RUN: pnacl-abicheck < %s | FileCheck %s

module asm "foo"
; CHECK: Module contains disallowed top-level inline assembly
