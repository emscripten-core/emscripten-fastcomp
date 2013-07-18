; RUN: pnacl-llc -mtriple=armv7a-none-nacl-gnueabi %s -filetype=obj \
; RUN:  -relocation-model=pic  -reduce-memory-footprint -sfi-disable-cp \
; RUN:  -sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data -mattr=+neon \
; RUN:  -O0 -disable-fp-elim -mcpu=cortex-a9 -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

; This test exercises -sfi-disable-cp together with -relocation-model=pic,
; to see that a movw/movt is actually generated as expected.

%struct.object = type { [16 x i8*] }

@__do_eh_ctor.object = internal global %struct.object zeroinitializer, align 8
@__EH_FRAME_BEGIN__ = internal global [0 x i8] zeroinitializer, section ".eh_frame", align 4
@llvm.global_ctors = appending global [1 x { i32, void ()* }] [{ i32, void ()* } { i32 65535, void ()* @__do_eh_ctor }]

define void @__register_frame_info(i8* %begin, %struct.object* %ob) {
entry:
  %begin.addr = alloca i8*, align 4
  %ob.addr = alloca %struct.object*, align 4
  store i8* %begin, i8** %begin.addr, align 4
  store %struct.object* %ob, %struct.object** %ob.addr, align 4
  ret void
}

define internal void @__do_eh_ctor() {
entry:
  call void @__register_frame_info(i8* getelementptr inbounds ([0 x i8]* @__EH_FRAME_BEGIN__, i32 0, i32 0), %struct.object* @__do_eh_ctor.object)
; llvm-objdump doesn't currently show the function label on ARM. Until it
; learns to do that, do a hacky 'bx lr' check to verify it's in the 2nd
; function, not the first

; CHECK: bx lr
; CHECK: movw
; CHECK-NEXT: movt

  ret void
}

