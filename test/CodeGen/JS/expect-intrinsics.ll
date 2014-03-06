; RUN: llc -march=js < %s | FileCheck %s

; Handle the llvm.expect intrinsic.

; CHECK: if (((($x)|0)!=(0)))
define void @foo(i32 %x) {
entry:
  %expval = call i32 @llvm.expect.i32(i32 %x, i32 0)
  %tobool = icmp ne i32 %expval, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @callee()
  br label %if.end

if.end:
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @llvm.expect.i32(i32, i32) #0

declare void @callee()

attributes #0 = { nounwind readnone }
