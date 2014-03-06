; RUN: llc -march=js < %s | FileCheck %s

; Phi lowering should check for dependency cycles, including looking through
; bitcasts, and emit extra copies as needed.

; CHECK: while(1) {
; CHECK:   $k$phi = $j;$j$phi = $k;$k = $k$phi;$j = $j$phi;
; CHECK: }
define void @foo(float* nocapture %p, i32* %j.init, i32* %k.init) {
entry:
  br label %for.body

for.body:
  %j = phi i32* [ %j.init, %entry ], [ %k.cast, %more ]
  %k = phi i32* [ %k.init, %entry ], [ %j.cast, %more ]
  br label %more

more:
  %j.cast = bitcast i32* %j to i32*
  %k.cast = bitcast i32* %k to i32*
  br label %for.body
}
