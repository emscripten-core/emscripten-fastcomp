; RUN: opt < %s -expand-constant-expr -S | FileCheck %s

@global_var1 = global i32 123
@global_var2 = global i32 123


define i8* @constantexpr_bitcast() {
  ret i8* bitcast (i32* @global_var1 to i8*)
}
; CHECK: @constantexpr_bitcast
; CHECK: %expanded = bitcast i32* @global_var1 to i8*
; CHECK: ret i8* %expanded


define i32 @constantexpr_nested() {
  ret i32 add (i32 ptrtoint (i32* @global_var1 to i32),
               i32 ptrtoint (i32* @global_var2 to i32))
}
; CHECK: @constantexpr_nested
; CHECK: %expanded1 = ptrtoint i32* @global_var1 to i32
; CHECK: %expanded2 = ptrtoint i32* @global_var2 to i32
; CHECK: %expanded = add i32 %expanded1, %expanded2
; CHECK: ret i32 %expanded


define i32 @constantexpr_nested2() {
  ret i32 mul (i32 add (i32 ptrtoint (i32* @global_var1 to i32),
                        i32 ptrtoint (i32* @global_var2 to i32)), i32 2)
}
; CHECK: @constantexpr_nested2
; CHECK: %expanded2 = ptrtoint i32* @global_var1 to i32
; CHECK: %expanded3 = ptrtoint i32* @global_var2 to i32
; CHECK: %expanded1 = add i32 %expanded2, %expanded3
; CHECK: %expanded = mul i32 %expanded1, 2
; CHECK: ret i32 %expanded


define i32 @constantexpr_phi() {
entry:
  br label %label
label:
  %result = phi i32 [ ptrtoint (i32* @global_var1 to i32), %entry ]
  ret i32 %result
}
; CHECK: @constantexpr_phi
; CHECK: entry:
; CHECK: %expanded = ptrtoint i32* @global_var1 to i32
; CHECK: br label %label
; CHECK: label:
; CHECK: %result = phi i32 [ %expanded, %entry ]


; This tests that ExpandConstantExpr correctly handles a PHI node that
; contains the same ConstantExpr twice.
; Using replaceAllUsesWith() is not correct on a PHI node when the
; new instruction has to be added to an incoming block.
define i32 @constantexpr_phi_twice(i1 %arg) {
  br i1 %arg, label %iftrue, label %iffalse
iftrue:
  br label %exit
iffalse:
  br label %exit
exit:
  %result = phi i32 [ ptrtoint (i32* @global_var1 to i32), %iftrue ],
                    [ ptrtoint (i32* @global_var1 to i32), %iffalse ]
  ret i32 %result
}
; CHECK: @constantexpr_phi_twice
; CHECK: iftrue:
; CHECK: %expanded = ptrtoint i32* @global_var1 to i32
; CHECK: iffalse:
; CHECK: %expanded1 = ptrtoint i32* @global_var1 to i32
; CHECK: exit:


define i32 @constantexpr_phi_multiple_entry(i1 %arg) {
entry:
  br i1 %arg, label %done, label %done
done:
  %result = phi i32 [ ptrtoint (i32* @global_var1 to i32), %entry ],
                    [ ptrtoint (i32* @global_var1 to i32), %entry ]
  ret i32 %result
}
; CHECK: @constantexpr_phi_multiple_entry
; CHECK: entry:
; CHECK: %expanded = ptrtoint i32* @global_var1 to i32
; CHECK: br i1 %arg, label %done, label %done
; CHECK: done:
; CHECK: %result = phi i32 [ %expanded, %entry ], [ %expanded, %entry ]



declare void @external_func()
declare void @personality_func()

define void @test_landingpad() {
  invoke void @external_func() to label %ok unwind label %onerror
ok:
  ret void
onerror:
  %lp = landingpad i32
      personality i8* bitcast (void ()* @personality_func to i8*)
      catch i32* null
  ret void
}
; landingpad can only accept a ConstantExpr, so this should remain
; unmodified.
; CHECK: @test_landingpad
; CHECK: personality i8* bitcast (void ()* @personality_func to i8*)
