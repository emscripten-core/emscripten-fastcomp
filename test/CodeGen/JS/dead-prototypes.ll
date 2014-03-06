; RUN: llc -march=js < %s | not grep printf

; llc shouldn't emit any code or bookkeeping for unused declarations.

define void @foo() {
  ret void
}

declare i32 @printf(i8* nocapture, ...)
