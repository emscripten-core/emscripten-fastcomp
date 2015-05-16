; RUN: opt < %s -nacl-expand-tls-constant-expr -S | FileCheck %s

@real_tvar = thread_local global i32 123
@tvar_alias = alias i32* @real_tvar
@tvar_alias2 = alias i32* getelementptr (i32, i32* @real_tvar, i32 100)


define i32* @get_tvar() {
  ret i32* @tvar_alias
}
; CHECK: define i32* @get_tvar()
; CHECK: ret i32* @real_tvar


define i32* @get_tvar2() {
  ret i32* @tvar_alias2
}
; CHECK: define i32* @get_tvar2()
; CHECK: %expanded = getelementptr i32, i32* @real_tvar, i32 100
; CHECK: ret i32* %expanded


define i32* @get_tvar3() {
  ret i32* getelementptr (i32, i32* @tvar_alias2, i32 100)
}
; CHECK: define i32* @get_tvar3()
; CHECK: %expanded = getelementptr i32, i32* @real_tvar, i32 200
; CHECK: ret i32* %expanded
