/*
  Object file built using:
  pnacl-clang -S -O0 -emit-llvm -o pnacl-hides-sandbox-x86-64.ll \
      pnacl-hides-sandbox-x86-64.c
  Then the comments below should be pasted into the .ll file.

; RUN: pnacl-llc -O2 -mtriple=x86_64-none-nacl -filetype=obj < %s | \
; RUN:     llvm-objdump -d -r - | FileCheck %s
; RUN: pnacl-llc -O2 -mtriple=x86_64-none-nacl -filetype=obj < %s | \
; RUN:     llvm-objdump -d -r - | FileCheck %s --check-prefix=NOCALLRET
;
; CHECK: TestDirectCall:
; Push the immediate return address
; CHECK:      pushq $0
; CHECK-NEXT: .text
; Immediate jump to the target
; CHECK:      jmpq 0
; CHECK-NEXT: DirectCallTarget
; Return label
; CHECK:      DirectCallRetAddr
;
; CHECK: TestIndirectCall:
; Push the immediate return address
; CHECK:      pushq $0
; CHECK-NEXT: .text
; Fixed sequence for indirect jump
; CHECK:      andl $-32, %r11d
; CHECK-NEXT: addq %r15, %r11
; CHECK-NEXT: jmpq *%r11
; Return label
; CHECK:      IndirectCallRetAddr
;
; Verify that the old frame pointer isn't leaked when saved
; CHECK: TestMaskedFramePointer:
; CHECK: movl    %ebp, %eax
; CHECK: pushq   %rax
; CHECK: movq    %rsp, %rbp
;
; Verify use of r10 instead of rax in the presence of varargs,
; when saving the old rbp.
; CHECK: TestMaskedFramePointerVarargs:
; CHECK: movl    %ebp, %r10d
; CHECK: pushq   %r10
; CHECK: movq    %rsp, %rbp
;
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
;
; Return sequence is just the indirect jump sequence
; CHECK: TestReturn:
; CHECK:      andl $-32, %r11d
; CHECK-NEXT: addq %r15, %r11
; CHECK-NEXT: jmpq *%r11
;
; Special test that no "call" or "ret" instructions are generated.
; NOCALLRET-NOT: call
; NOCALLRET-NOT: ret
*/

#include <stdlib.h>
#include <stdio.h>

void TestDirectCall(void) {
  extern void DirectCallTarget(void);
  DirectCallTarget();
}

void TestIndirectCall(void) {
  extern void (*IndirectCallTarget)(void);
  IndirectCallTarget();
}

void TestMaskedFramePointer(int Arg) {
  extern void Consume(void *);
  // Calling alloca() is one way to force the rbp frame pointer.
  void *Tmp = alloca(Arg);
  Consume(Tmp);
}

void TestMaskedFramePointerVarargs(int Arg, ...) {
  extern void Consume(void *);
  void *Tmp = alloca(Arg);
  Consume(Tmp);
}

void TestIndirectJump(int Arg) {
  switch (Arg) {
  case 2:
    puts("Prime 1");
    break;
  case 3:
    puts("Prime 2");
    break;
  case 5:
    puts("Prime 3");
    break;
  case 7:
    puts("Prime 4");
    break;
  case 11:
    puts("Prime 5");
    break;
  }
}

void TestReturn(void) {
}
