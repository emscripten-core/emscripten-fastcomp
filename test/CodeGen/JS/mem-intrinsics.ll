; RUN: llc < %s | FileCheck %s

; llc should emit small aligned memcpy and memset inline.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: test_unrolled_memcpy
; CHECK: HEAP32[$d>>2]=HEAP32[$s>>2]|0;HEAP32[$d+4>>2]=HEAP32[$s+4>>2]|0;HEAP32[$d+8>>2]=HEAP32[$s+8>>2]|0;HEAP32[$d+12>>2]=HEAP32[$s+12>>2]|0;HEAP32[$d+16>>2]=HEAP32[$s+16>>2]|0;HEAP32[$d+20>>2]=HEAP32[$s+20>>2]|0;HEAP32[$d+24>>2]=HEAP32[$s+24>>2]|0;HEAP32[$d+28>>2]=HEAP32[$s+28>>2]|0;
define void @test_unrolled_memcpy(i8* %d, i8* %s) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %d, i8* %s, i32 32, i32 4, i1 false)
  ret void
}

; CHECK: test_loop_memcpy
; CHECK: dest=$d; src=$s; stop=dest+64|0; do { HEAP32[dest>>2]=HEAP32[src>>2]|0; dest=dest+4|0; src=src+4|0; } while ((dest|0) < (stop|0))
define void @test_loop_memcpy(i8* %d, i8* %s) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %d, i8* %s, i32 64, i32 4, i1 false)
  ret void
}

; CHECK: test_call_memcpy
; CHECK: memcpy(($d|0),($s|0),65536)
define void @test_call_memcpy(i8* %d, i8* %s) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %d, i8* %s, i32 65536, i32 4, i1 false)
  ret void
}

; CHECK: test_unrolled_memset
; CHECK:  HEAP32[$d>>2]=0|0;HEAP32[$d+4>>2]=0|0;HEAP32[$d+8>>2]=0|0;HEAP32[$d+12>>2]=0|0;HEAP32[$d+16>>2]=0|0;HEAP32[$d+20>>2]=0|0;HEAP32[$d+24>>2]=0|0;HEAP32[$d+28>>2]=0|0;
define void @test_unrolled_memset(i8* %d, i8* %s) {
  call void @llvm.memset.p0i8.i32(i8* %d, i8 0, i32 32, i32 4, i1 false)
  ret void
}

; CHECK: test_loop_memset
; CHECK: dest=$d; stop=dest+64|0; do { HEAP32[dest>>2]=0|0; dest=dest+4|0; } while ((dest|0) < (stop|0));
define void @test_loop_memset(i8* %d, i8* %s) {
  call void @llvm.memset.p0i8.i32(i8* %d, i8 0, i32 64, i32 4, i1 false)
  ret void
}

; CHECK: test_call_memset
; CHECK: memset(($d|0),0,65536)
define void @test_call_memset(i8* %d, i8* %s) {
  call void @llvm.memset.p0i8.i32(i8* %d, i8 0, i32 65536, i32 4, i1 false)
  ret void
}

; Also, don't emit declarations for the intrinsic functions.
; CHECK-NOT: p0i8

declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture, i32, i32, i1) #0
declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1) #0

attributes #0 = { nounwind }
