; RUN: pnacl-llc -mtriple=armv7a-none-nacl-gnueabi %s -filetype=obj \
; RUN:  -relocation-model=pic -sfi-disable-cp -mattr=+neon \
; RUN:  -O0 -mcpu=cortex-a9 -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

; This test exercises -sfi-disable-cp together with -relocation-model=pic,
; to see that a movw/movt is actually generated as expected.
; Use -O0 so that movw/movt are scheduled to be adjacent,
; instead of having other independent instructions interleaved.

%struct.object = type { [16 x i8*] }

@__do_eh_ctor.object = internal global %struct.object zeroinitializer, align 8
@__EH_FRAME_BEGIN__ = internal global [0 x i8] zeroinitializer, section ".eh_frame", align 4
@llvm.global_ctors = appending global [1 x { i32, void ()* }] [{ i32, void ()* } { i32 65535, void ()* @__do_eh_ctor }]

declare void @__register_frame_info(i8* %begin, %struct.object* %ob)

define internal void @__do_eh_ctor() {
entry:
  call void @__register_frame_info(i8* getelementptr inbounds ([0 x i8]* @__EH_FRAME_BEGIN__, i32 0, i32 0), %struct.object* @__do_eh_ctor.object)
; CHECK-LABEL: __do_eh_ctor
; CHECK: movw
; CHECK-NEXT: movt
; CHECK-NEXT: add r{{.*}}, pc, r{{.*}}

  ret void
}

