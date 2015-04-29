; RUN: llc -mtriple=arm-darwin  -mattr=v6 < %s | FileCheck %s
; RUN: llc -mtriple=thumb-darwin  -mattr=v6 < %s | FileCheck %s

; @LOCALMOD Change to test that NaCl doesn't merge the pattern.
; RUN: llc -mtriple=armv7-unknown-nacl < %s | FileCheck %s -check-prefix=NACL


define void @test1(i16* nocapture %data) {
entry:
  %0 = load i16, i16* %data, align 2
  %1 = tail call i16 @llvm.bswap.i16(i16 %0)
  store i16 %1, i16* %data, align 2
  ret void

  ; CHECK-LABEL: test1:
  ; CHECK: ldrh r[[R1:[0-9]+]], [r0]
  ; CHECK: rev16 r[[R1]], r[[R1]]
  ; CHECK: strh r[[R1]], [r0]

  ; NACL-LABEL: test1:
  ; NACL: ldrh r[[R1:[0-9]+]], [r0]
  ; NACL: rev r[[R1]], r[[R1]]
  ; NACL: lsr r[[R1]], r[[R1]], #16
  ; NACL: strh r[[R1]], [r0]
}


define void @test2(i16* nocapture %data, i16 zeroext %in) {
entry:
  %0 = tail call i16 @llvm.bswap.i16(i16 %in)
  store i16 %0, i16* %data, align 2
  ret void

  ; CHECK-LABEL: test2:
  ; CHECK: rev16 r[[R1:[0-9]+]], r1
  ; CHECK: strh r[[R1]], [r0]

  ; NACL-LABEL: test2:
  ; NACL: rev16 r[[R1:[0-9]+]], r1
  ; NACL: strh r[[R1]], [r0]
}


define i16 @test3(i16* nocapture %data) {
entry:
  %0 = load i16, i16* %data, align 2
  %1 = tail call i16 @llvm.bswap.i16(i16 %0)
  ret i16 %1

  ; CHECK-LABEL: test3:
  ; CHECK: ldrh r[[R0:[0-9]+]], [r0]
  ; CHECK: rev16 r[[R0]], r0

  ; NACL-LABEL: test3:
  ; NACL: ldrh r[[R0:[0-9]+]], [r0]
  ; NACL: rev r[[R0]], r0
  ; NACL: lsr r[[R0]], r0, #16
}

define i16 @test4(i32 %in) {
  %1 = add i32 %in, 256
  %2 = inttoptr i32 %1 to i32*
  %3 = load i32, i32* %2, align 2
  %4 = trunc i32 %3 to i16
  %5 = tail call i16 @llvm.bswap.i16(i16 %4)
  %6 = add i32 %in, 258
  %7 = inttoptr i32 %6 to i16*
  store i16 %5, i16* %7, align 2
  ret i16 %5

  ; CHECK-LABEL: test4:
  ; CHECK: ldrh r[[R1:[0-9]+]], [r0, r1]
  ; CHECK: rev16 r[[R2:[0-9]+]], r[[R1]]
  ; CHECK: strh r[[R2]], [r0, r3]

  ; NACL-LABEL: test4:
  ; NACL: add r[[R0:[0-9]+]], r0, #256
  ; NACL: ldrh r[[R0]], [r0]
  ; NACL: rev r[[R0]], r[[R0]]
  ; NACL: lsr r[[R0]], r[[R0]], #16
  ; NACL: strh r[[R0]], [r1]
}

declare i16 @llvm.bswap.i16(i16)
