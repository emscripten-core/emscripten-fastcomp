; Test forward reference of a pointer-typed intrinsic result.

; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-thaw -allow-local-symbol-tables \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD2

declare i8* @llvm.nacl.read.tp()

define i32 @forward_ref() {
  br label %block1

block2:
  %1 = load i8* %3
  %2 = ptrtoint i8* %3 to i32
  ret i32 %2

block1:
  %3 = call i8* @llvm.nacl.read.tp()
  br label %block2
}

; TD2:      define i32 @forward_ref() {
; TD2-NEXT:   br label %block1
; TD2:      block2:
; TD2-NEXT:   %1 = inttoptr i32 %4 to i8*
; TD2-NEXT:   %2 = load i8* %1
; TD2-NEXT:   ret i32 %4
; TD2:      block1:
; TD2-NEXT:   %3 = call i8* @llvm.nacl.read.tp()
; TD2-NEXT:   %4 = ptrtoint i8* %3 to i32
; TD2-NEXT:   br label %block2
; TD2-NEXT: }
