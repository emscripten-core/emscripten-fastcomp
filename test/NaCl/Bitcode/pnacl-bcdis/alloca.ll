; Test alloca instructions.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

define void @AllocaTests(i32 %p0) {

; CHECK:               |                             |  %b0:

  %v0 = alloca i8, i32 1, align 4
  %v1 = alloca i8, i32 4, align 4
  %v2 = alloca i8, i32 %p0, align 128
  ret void

; CHECK-NEXT:    {{.*}}|    3: <19, 2, 3>            |    %v0 = alloca i8, i32 %c0, align 4;
; CHECK-NEXT:    {{.*}}|    3: <19, 2, 3>            |    %v1 = alloca i8, i32 %c1, align 4;
; CHECK-NEXT:    {{.*}}|    3: <19, 5, 8>            |    %v2 = alloca i8, i32 %p0, 
; CHECK-NEXT:          |                             |        align 128;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}
