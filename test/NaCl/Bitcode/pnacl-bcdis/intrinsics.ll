; Tests calls to intrinsics.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; Test simple intrinsic call.
declare i8* @llvm.nacl.read.tp()

define i32 @ReturnPtrIntrinsic() {
  %1 = call i8* @llvm.nacl.read.tp()
  %2 = ptrtoint i8* %1 to i32
  ret i32 %2
}

; Test intrinsic call with i1 argument.
declare external i32 @llvm.ctlz.i32(i32, i1)

define i32 @f() {
  %v0 = call i32 @llvm.ctlz.i32(i32 0, i1 0)
  ret i32 %v0
}

; CHECK-NOT: Error


