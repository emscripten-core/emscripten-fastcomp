; RUN: opt < %s -add-pnacl-external-decls -S | FileCheck %s

declare void @foobar(i32)

; CHECK: declare i32 @setjmp(i8*)
; CHECK: declare void @longjmp(i8*, i32)
