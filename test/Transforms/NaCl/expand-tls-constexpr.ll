; RUN: opt < %s -nacl-expand-tls-constant-expr -S | FileCheck %s

@tvar = thread_local global i32 0


define i32 @test_converting_ptrtoint() {
  ret i32 ptrtoint (i32* @tvar to i32)
}
; CHECK: define i32 @test_converting_ptrtoint()
; CHECK: %expanded = ptrtoint i32* @tvar to i32
; CHECK: ret i32 %expanded


define i32 @test_converting_add() {
  ret i32 add (i32 ptrtoint (i32* @tvar to i32), i32 4)
}
; CHECK: define i32 @test_converting_add()
; CHECK: %expanded1 = ptrtoint i32* @tvar to i32
; CHECK: %expanded = add i32 %expanded1, 4
; CHECK: ret i32 %expanded


define i32 @test_converting_multiple_operands() {
  ret i32 add (i32 ptrtoint (i32* @tvar to i32),
               i32 ptrtoint (i32* @tvar to i32))
}
; CHECK: define i32 @test_converting_multiple_operands()
; CHECK: %expanded1 = ptrtoint i32* @tvar to i32
; CHECK: %expanded = add i32 %expanded1, %expanded1
; CHECK: ret i32 %expanded


define i32 @test_allocating_new_var_name(i32 %expanded) {
  %result = add i32 %expanded, ptrtoint (i32* @tvar to i32)
  ret i32 %result
}
; CHECK: define i32 @test_allocating_new_var_name(i32 %expanded)
; CHECK: %expanded1 = ptrtoint i32* @tvar to i32
; CHECK: %result = add i32 %expanded, %expanded1
; CHECK: ret i32 %result


define i8* @test_converting_bitcast() {
  ret i8* bitcast (i32* @tvar to i8*)
}
; CHECK: define i8* @test_converting_bitcast()
; CHECK: %expanded = bitcast i32* @tvar to i8*
; CHECK: ret i8* %expanded


define i32* @test_converting_getelementptr() {
  ; Use an index >1 to ensure that "inbounds" is not added automatically.
  ret i32* getelementptr (i32* @tvar, i32 2)
}
; CHECK: define i32* @test_converting_getelementptr()
; CHECK: %expanded = getelementptr i32* @tvar, i32 2
; CHECK: ret i32* %expanded


; This is identical to @test_converting_getelementptr().
; We need to check that both copies of getelementptr are fixed.
define i32* @test_converting_getelementptr_copy() {
  ret i32* getelementptr (i32* @tvar, i32 2)
}
; CHECK: define i32* @test_converting_getelementptr_copy()
; CHECK: %expanded = getelementptr i32* @tvar, i32 2
; CHECK: ret i32* %expanded


define i32* @test_converting_getelementptr_inbounds() {
  ret i32* getelementptr inbounds (i32* @tvar, i32 2)
}
; CHECK: define i32* @test_converting_getelementptr_inbounds()
; CHECK: %expanded = getelementptr inbounds i32* @tvar, i32 2
; CHECK: ret i32* %expanded


define i32* @test_converting_phi(i1 %cmp) {
entry:
  br i1 %cmp, label %return, label %else

else:
  br label %return

return:
  %result = phi i32* [ getelementptr (i32* @tvar, i32 1), %entry ], [ null, %else ]
  ret i32* %result
}
; The converted ConstantExprs get pushed back into the PHI node's
; incoming block, which might be suboptimal but works in all cases.
; CHECK: define i32* @test_converting_phi(i1 %cmp)
; CHECK: entry:
; CHECK: %expanded = getelementptr inbounds i32* @tvar, i32 1
; CHECK: else:
; CHECK: return:
; CHECK: %result = phi i32* [ %expanded, %entry ], [ null, %else ]


@addr1 = global i8* blockaddress(@test_converting_phi_with_indirectbr, %return)
@addr2 = global i8* blockaddress(@test_converting_phi_with_indirectbr, %else)
define i32* @test_converting_phi_with_indirectbr(i8* %addr) {
entry:
  indirectbr i8* %addr, [ label %return, label %else ]

else:
  br label %return

return:
  %result = phi i32* [ getelementptr (i32* @tvar, i32 1), %entry ], [ null, %else ]
  ret i32* %result
}
; CHECK: define i32* @test_converting_phi_with_indirectbr(i8* %addr)
; CHECK: entry:
; CHECK: %expanded = getelementptr inbounds i32* @tvar, i32 1
; CHECK: return:
; CHECK: %result = phi i32* [ %expanded, %entry ], [ null, %else ]
