; RUN: llc -mtriple=armv7 < %s | FileCheck %s
; RUN: llc -mtriple=armv7-unknown-nacl < %s | FileCheck %s -check-prefix=NACL

; Make sure NaCl doesn't generate register-register loads, which are disallowed
; by its sandbox.

@i16_large = internal global [516 x i8] undef

define void @test() {
  %i = ptrtoint [516 x i8]* @i16_large to i32
  %a = add i32 %i, 512
  %a.asptr = inttoptr i32 %a to i16*
  %loaded = load atomic i16, i16* %a.asptr seq_cst, align 2
  ret void
}
; CHECK-LABEL: test:
; CHECK: ldrh	r0, [r1, r0]

; NACL-LABEL: test:
; NACL: movw	r0, :lower16:i16_large
; NACL: movt	r0, :upper16:i16_large
; NACL: add	r0, r0, #512
; NACL: ldrh	r0, [r0]
; NACL: dmb	ish
