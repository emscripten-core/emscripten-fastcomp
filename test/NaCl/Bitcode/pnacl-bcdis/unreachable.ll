; Test unreachable statement (without worrying if legal in context).

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s



; Note: This code only tests parsing of unreachable, not correctness
; of use.
define void @TestUnreach(i32 %p0) {

; CHECK:               |                             |  %b0:

  %v0 = trunc i32 %p0 to i1
  br i1 %v0, label %b1, label %b2

b1:
  br i1 %v0, label %b3, label %b4

b2:
  br i1 %v0, label %b5, label %b3


b3:
  unreachable

; CHECK:               |                             |  %b3:
; CHECK-NEXT:    {{.*}}|    3: <15>                  |    unreachable;

b4:
  unreachable

; CHECK:               |                             |  %b4:
; CHECK-NEXT:    {{.*}}|    3: <15>                  |    unreachable;

b5:
  unreachable

; CHECK:               |                             |  %b5:
; CHECK-NEXT:    {{.*}}|    3: <15>                  |    unreachable;

}
