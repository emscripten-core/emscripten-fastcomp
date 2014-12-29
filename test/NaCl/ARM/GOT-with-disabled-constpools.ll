; RUN: pnacl-llc -mtriple=armv7a-none-nacl-gnueabi %s -filetype=obj \
; RUN:  -relocation-model=pic -mattr=+neon \
; RUN:  -O0 -mcpu=cortex-a9 -o - \
; RUN:  | llvm-objdump -disassemble -r -triple armv7 - | FileCheck %s

; This test exercises NaCl (which doesn't use constant islands)
; together with -relocation-model=pic, to see that a movw/movt
; is actually generated as expected. Use -O0 so that movw/movt
; are scheduled to be adjacent, instead of having other independent
; instructions interleaved.

%struct.object = type { [16 x i8*] }

@__do_eh_ctor.object = internal global %struct.object zeroinitializer, align 8
@__EH_FRAME_BEGIN__ = internal global [0 x i8] zeroinitializer, section ".eh_frame", align 4
@llvm.global_ctors = appending global [1 x { i32, void ()* }] [{ i32, void ()* } { i32 65535, void ()* @__do_eh_ctor }]

declare void @__register_frame_info(i8* %begin, %struct.object* %ob)

define internal void @__do_eh_ctor() {
entry:
  call void @__register_frame_info(i8* getelementptr inbounds ([0 x i8]* @__EH_FRAME_BEGIN__, i32 0, i32 0), %struct.object* @__do_eh_ctor.object)
; CHECK-LABEL: __do_eh_ctor

; CHECK: movw r[[REG0:[0-9]+]]
; CHECK-NEXT: R_ARM_MOVW_PREL_NC .LCPI
; CHECK-NEXT: movt r[[REG0]]
; CHECK-NEXT: R_ARM_MOVT_PREL .LCPI
; CHECK-NEXT: add r[[REG0]], pc, r[[REG0]]
; CHECK: ldr r[[REG0]], {{\[}}r[[REG0]]{{\]}}

; CHECK: movw r[[REG1:[0-9]+]]
; CHECK-NEXT: R_ARM_MOVW_PREL_NC _GLOBAL_OFFSET_TABLE_
; CHECK-NEXT: movt r[[REG1]]
; CHECK-NEXT: R_ARM_MOVT_PREL _GLOBAL_OFFSET_TABLE_
; CHECK-NEXT: add r[[REG1]], pc, r[[REG1]]
; CHECK: add r[[REG0]], r[[REG0]], r[[REG1]]

; CHECK: movw r[[REG2:[0-9]+]]
; CHECK-NEXT: R_ARM_MOVW_PREL_NC .LCPI
; CHECK-NEXT: movt r[[REG2]]
; CHECK-NEXT: R_ARM_MOVT_PREL .LCPI
; CHECK-NEXT: add r[[REG2]], pc, r[[REG2]]
; CHECK: ldr r[[REG2]], {{\[}}r[[REG2]]{{\]}}
; CHECK: add r[[REG1]], r[[REG2]], r[[REG1]]

  ret void
}

