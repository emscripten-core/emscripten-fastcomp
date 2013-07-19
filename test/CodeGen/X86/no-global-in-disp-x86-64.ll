; RUN: pnacl-llc -O2 -mtriple=x86_64-none-nacl < %s | \
; RUN:     FileCheck %s --check-prefix=NACLON
; RUN: pnacl-llc -O2 -mtriple=x86_64-linux     < %s | \
; RUN:     FileCheck %s --check-prefix=NACLOFF

; This test is derived from the following C code:
;
; int myglobal[100];
; void test(int arg)
; {
;   myglobal[arg] = arg;
;   myglobal[arg+1] = arg;
; }
; int main(int argc, char **argv)
; {
;   test(argc);
; }
;
; The goal is NOT to produce an instruction with "myglobal" as the
; displacement value in any addressing mode, e.g. this (bad) instruction:
;
; movl %eax, %nacl:myglobal(%r15,%rax,4)
;
; The NACLOFF test is a canary that tries to ensure that the NACLON test is
; testing the right thing.  If the NACLOFF test starts failing, it's likely
; that the LLVM -O2 optimizations are no longer generating the problematic
; pattern that NACLON tests for.  In that case, the test should be modified.


@myglobal = global [100 x i32] zeroinitializer, align 4

define void @test(i32 %arg) #0 {
entry:
; NACLON: test:
; NACLON-NOT: mov{{.*}}nacl:myglobal(
; NACLOFF: test:
; NACLOFF: mov{{.*}}myglobal(
  %arg.addr = alloca i32, align 4
  store i32 %arg, i32* %arg.addr, align 4
  %0 = load i32* %arg.addr, align 4
  %1 = load i32* %arg.addr, align 4
  %arrayidx = getelementptr inbounds [100 x i32]* @myglobal, i32 0, i32 %1
  store i32 %0, i32* %arrayidx, align 4
  %2 = load i32* %arg.addr, align 4
  %3 = load i32* %arg.addr, align 4
  %add = add nsw i32 %3, 1
  %arrayidx1 = getelementptr inbounds [100 x i32]* @myglobal, i32 0, i32 %add
  store i32 %2, i32* %arrayidx1, align 4
  ret void
}
