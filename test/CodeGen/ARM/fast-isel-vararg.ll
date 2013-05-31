; RUN: llc < %s -O0 -verify-machineinstrs -fast-isel-abort -relocation-model=dynamic-no-pic -mtriple=armv7-apple-ios | FileCheck %s --check-prefix=ARM-IOS
; RUN: llc < %s -O0 -verify-machineinstrs -fast-isel-abort -relocation-model=dynamic-no-pic -mtriple=thumbv7-apple-ios | FileCheck %s --check-prefix=THUMB

define i32 @VarArg() nounwind {
entry:
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %k = alloca i32, align 4
  %m = alloca i32, align 4
  %n = alloca i32, align 4
  %tmp = alloca i32, align 4
  %0 = load i32* %i, align 4
  %1 = load i32* %j, align 4
  %2 = load i32* %k, align 4
  %3 = load i32* %m, align 4
  %4 = load i32* %n, align 4
; ARM-IOS: VarArg
; ARM-IOS: mov r7, sp
; ARM-IOS: sub sp, sp, #32
; ARM-IOS: movw r0, #5
; ARM-IOS: ldr r1, [r7, #-4]
; ARM-IOS: ldr r2, [r7, #-8]
; ARM-IOS: ldr r3, [r7, #-12]
; ARM-IOS: ldr r9, [sp, #16]
; ARM-IOS: ldr r12, [sp, #12]
; ARM-IOS: str r9, [sp]
; ARM-IOS: str r12, [sp, #4]
; ARM-IOS: bl _CallVariadic
; THUMB: sub sp, #32
; THUMB: movs r0, #5
; THUMB: movt r0, #0
; THUMB: ldr r1, [sp, #28]
; THUMB: ldr r2, [sp, #24]
; THUMB: ldr r3, [sp, #20]
; THUMB: ldr.w {{[a-z0-9]+}}, [sp, #16]
; THUMB: ldr.w {{[a-z0-9]+}}, [sp, #12]
; THUMB: str.w {{[a-z0-9]+}}, [sp]
; THUMB: str.w {{[a-z0-9]+}}, [sp, #4]
; THUMB: bl {{_?}}CallVariadic
  %call = call i32 (i32, ...)* @CallVariadic(i32 5, i32 %0, i32 %1, i32 %2, i32 %3, i32 %4)
  store i32 %call, i32* %tmp, align 4
  %5 = load i32* %tmp, align 4
  ret i32 %5
}

declare i32 @CallVariadic(i32, ...)
