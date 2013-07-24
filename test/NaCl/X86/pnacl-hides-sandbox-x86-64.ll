; RUN: pnacl-llc -O2 -mtriple=x86_64-none-nacl -filetype=obj < %s | \
; RUN:     llvm-objdump -d -r - | FileCheck %s
; RUN: pnacl-llc -O2 -mtriple=x86_64-none-nacl -filetype=obj < %s | \
; RUN:     llvm-objdump -d -r - | FileCheck %s --check-prefix=NOCALLRET

; ModuleID = 'pnacl-hides-sandbox-x86-64.c'
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"

@IndirectCallTarget = external global void ()*
@.str = private unnamed_addr constant [8 x i8] c"Prime 1\00", align 1
@.str1 = private unnamed_addr constant [8 x i8] c"Prime 2\00", align 1
@.str2 = private unnamed_addr constant [8 x i8] c"Prime 3\00", align 1
@.str3 = private unnamed_addr constant [8 x i8] c"Prime 4\00", align 1
@.str4 = private unnamed_addr constant [8 x i8] c"Prime 5\00", align 1

; Function Attrs: nounwind
define void @TestDirectCall() #0 {
entry:
  call void @DirectCallTarget()
  ret void
}
; CHECK: TestDirectCall:
; Push the immediate return address
; CHECK:      pushq $0
; CHECK-NEXT: .text
; Immediate jump to the target
; CHECK:      jmpq 0
; CHECK-NEXT: DirectCallTarget

declare void @DirectCallTarget() #1

; Function Attrs: nounwind
define void @TestIndirectCall() #0 {
entry:
  %0 = load void ()** @IndirectCallTarget, align 4
  call void %0()
  ret void
}
; CHECK: TestIndirectCall:
; Push the immediate return address
; CHECK:      pushq $0
; CHECK-NEXT: .text
; Fixed sequence for indirect jump
; CHECK:      andl $-32, %r11d
; CHECK-NEXT: addq %r15, %r11
; CHECK-NEXT: jmpq *%r11

; Function Attrs: nounwind
define void @TestMaskedFramePointer(i32 %Arg) #0 {
entry:
  %Arg.addr = alloca i32, align 4
  %Tmp = alloca i8*, align 4
  store i32 %Arg, i32* %Arg.addr, align 4
  %0 = load i32* %Arg.addr, align 4
  %1 = alloca i8, i32 %0
  store i8* %1, i8** %Tmp, align 4
  %2 = load i8** %Tmp, align 4
  call void @Consume(i8* %2)
  ret void
}
; Verify that the old frame pointer isn't leaked when saved
; CHECK: TestMaskedFramePointer:
; CHECK: movl    %ebp, %eax
; CHECK: pushq   %rax
; CHECK: movq    %rsp, %rbp

declare void @Consume(i8*) #1

; Function Attrs: nounwind
define void @TestMaskedFramePointerVarargs(i32 %Arg, ...) #0 {
entry:
  %Arg.addr = alloca i32, align 4
  %Tmp = alloca i8*, align 4
  store i32 %Arg, i32* %Arg.addr, align 4
  %0 = load i32* %Arg.addr, align 4
  %1 = alloca i8, i32 %0
  store i8* %1, i8** %Tmp, align 4
  %2 = load i8** %Tmp, align 4
  call void @Consume(i8* %2)
  ret void
}
; Verify use of r10 instead of rax in the presence of varargs,
; when saving the old rbp.
; CHECK: TestMaskedFramePointerVarargs:
; CHECK: movl    %ebp, %r10d
; CHECK: pushq   %r10
; CHECK: movq    %rsp, %rbp

; Function Attrs: nounwind
define void @TestIndirectJump(i32 %Arg) #0 {
entry:
  %Arg.addr = alloca i32, align 4
  store i32 %Arg, i32* %Arg.addr, align 4
  %0 = load i32* %Arg.addr, align 4
  switch i32 %0, label %sw.epilog [
    i32 2, label %sw.bb
    i32 3, label %sw.bb1
    i32 5, label %sw.bb3
    i32 7, label %sw.bb5
    i32 11, label %sw.bb7
  ]

sw.bb:                                            ; preds = %entry
  %call = call i32 @puts(i8* getelementptr inbounds ([8 x i8]* @.str, i32 0, i32 0))
  br label %sw.epilog

sw.bb1:                                           ; preds = %entry
  %call2 = call i32 @puts(i8* getelementptr inbounds ([8 x i8]* @.str1, i32 0, i32 0))
  br label %sw.epilog

sw.bb3:                                           ; preds = %entry
  %call4 = call i32 @puts(i8* getelementptr inbounds ([8 x i8]* @.str2, i32 0, i32 0))
  br label %sw.epilog

sw.bb5:                                           ; preds = %entry
  %call6 = call i32 @puts(i8* getelementptr inbounds ([8 x i8]* @.str3, i32 0, i32 0))
  br label %sw.epilog

sw.bb7:                                           ; preds = %entry
  %call8 = call i32 @puts(i8* getelementptr inbounds ([8 x i8]* @.str4, i32 0, i32 0))
  br label %sw.epilog

sw.epilog:                                        ; preds = %entry, %sw.bb7, %sw.bb5, %sw.bb3, %sw.bb1, %sw.bb
  ret void
}
; Test the indirect jump sequence derived from a "switch" statement.
; CHECK: TestIndirectJump:
; CHECK:      andl $-32, %r11d
; CHECK-NEXT: addq %r15, %r11
; CHECK-NEXT: jmpq *%r11
; At least 4 "jmp"s due to 5 switch cases
; CHECK:      jmp
; CHECK:      jmp
; CHECK:      jmp
; CHECK:      jmp
; At least 1 direct call to puts()
; CHECK:      pushq $0
; CHECK-NEXT: .text
; CHECK:      jmpq 0
; CHECK-NEXT: puts

declare i32 @puts(i8*) #1

; Function Attrs: nounwind
define void @TestReturn() #0 {
entry:
  ret void
}
; Return sequence is just the indirect jump sequence
; CHECK: TestReturn:
; CHECK:      andl $-32, %r11d
; CHECK-NEXT: addq %r15, %r11
; CHECK-NEXT: jmpq *%r11

attributes #0 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }

; Special test that no "call" or "ret" instructions are generated.
; NOCALLRET-NOT: call
; NOCALLRET-NOT: ret
