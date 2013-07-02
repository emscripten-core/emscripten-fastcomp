; RUN: pnacl-abicheck < %s | FileCheck %s

@var = internal global [4 x i8] c"xxxx"


; CHECK-NOT: disallowed

define internal void @bad_cases() {
  ; ConstantExprs should be rejected here.
  switch i32 ptrtoint ([4 x i8]* @var to i32), label %next [i32 0, label %next]
; CHECK: disallowed: bad switch condition
next:

  ; Bad integer type.
  switch i32 0, label %next [i99 0, label %next]
; CHECK: bad switch case

  ; Bad integer type.
  switch i32 0, label %next [i32 0, label %next
                             i99 1, label %next]
; CHECK: bad switch case

  ; Note that the reader only allows ConstantInts in the label list.
  ; We don't need to check the following, because the reader rejects
  ; it:
  ; switch i32 0, label %next [i32 ptrtoint (i32* @ptr to i32), label %next]

  ret void
}

; CHECK-NOT: disallowed
