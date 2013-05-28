; RUN: llc -mtriple=i386-unknown-nacl -filetype=asm %s -o - \
; RUN:  | FileCheck %s --check-prefix=X86
; Test that @llvm.nacl.{set|long}jmp intrinsics calls get translated to library
; calls as expected.

declare i32 @llvm.nacl.setjmp(i8*)
declare void @llvm.nacl.longjmp(i8*, i32)

define void @foo(i8* %arg) {
  %num = call i32 @llvm.nacl.setjmp(i8* %arg)
; X86: naclcall setjmp

  call void @llvm.nacl.longjmp(i8* %arg, i32 %num)
; X86: naclcall longjmp

  ret void
}

