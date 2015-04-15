; RUN: opt < %s -resolve-aliases -S | FileCheck %s

; CHECK-NOT: @alias

@r1 = internal global i32 zeroinitializer
@a1 = alias i32* @r1
define i32* @usea1() {
; CHECK: ret i32* @r1
  ret i32* @a1
}

@funcalias = alias i32* ()* @usea1
; CHECK: @usefuncalias
define void @usefuncalias() {
; CHECK: call i32* @usea1
  %1 = call i32* @funcalias()
  ret void
}

@bc1 = global i8* bitcast (i32* @r1 to i8*)
@bcalias = alias i8* bitcast (i32* @r1 to i8*)

; CHECK: @usebcalias
define i8* @usebcalias() {
; CHECK: ret i8* bitcast (i32* @r1 to i8*)
  ret i8* @bcalias
}


@fa2 = alias i32* ()* @funcalias
; CHECK: @usefa2
define void @usefa2() {
; CHECK: call i32* @usea1
  call i32* @fa2()
  ret void
}
