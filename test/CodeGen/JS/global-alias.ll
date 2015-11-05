; RUN: llc < %s | FileCheck %s

; Handle global aliases of various kinds.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

@pri = internal global [60 x i8] zeroinitializer
@pub = global [60 x i8] zeroinitializer

@pri_int = internal alias [60 x i8], [60 x i8]* @pri
@pri_wea = weak alias [60 x i8], [60 x i8]* @pri
@pri_nor = alias [60 x i8], [60 x i8]* @pri

@pub_int = internal alias [60 x i8], [60 x i8]* @pub
@pub_wea = weak alias [60 x i8], [60 x i8]* @pub
@pub_nor = alias [60 x i8], [60 x i8]* @pub

; CHECK: test0(
; CHECK: return ([[PRI:[0-9]+]]|0);
define [60 x i8]* @test0() {
  ret [60 x i8]* @pri
}
; CHECK: test1(
; CHECK: return ([[PRI]]|0);
define [60 x i8]* @test1() {
  ret [60 x i8]* @pri_int
}
; CHECK: test2(
; CHECK: return ([[PRI]]|0);
define [60 x i8]* @test2() {
  ret [60 x i8]* @pri_wea
}
; CHECK: test3(
; CHECK: return ([[PRI]]|0);
define [60 x i8]* @test3() {
  ret [60 x i8]* @pri_nor
}

; CHECK: test4(
; CHECK: return ([[PUB:[0-9]+]]|0);
define [60 x i8]* @test4() {
  ret [60 x i8]* @pub
}
; CHECK: test5(
; CHECK: return ([[PUB]]|0);
define [60 x i8]* @test5() {
  ret [60 x i8]* @pub_int
}
; CHECK: test6(
; CHECK: return ([[PUB]]|0);
define [60 x i8]* @test6() {
  ret [60 x i8]* @pub_wea
}
; CHECK: test7(
; CHECK: return ([[PUB]]|0);
define [60 x i8]* @test7() {
  ret [60 x i8]* @pub_nor
}
