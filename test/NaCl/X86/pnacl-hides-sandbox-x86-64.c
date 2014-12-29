/*
  Object file built using:
  pnacl-clang -S -O0 -emit-llvm -o pnacl-hides-sandbox-x86-64.ll \
      pnacl-hides-sandbox-x86-64.c
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
