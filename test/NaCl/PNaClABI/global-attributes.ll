; RUN: pnacl-abicheck < %s | FileCheck %s

; Global variable attributes

; CHECK: Variable var_with_section has disallowed "section" attribute
@var_with_section = global [1 x i8] zeroinitializer, section ".some_section"

; PNaCl programs can depend on data alignments in general, so we allow
; "align" on global variables.
; CHECK-NOT: var_with_alignment
@var_with_alignment = global [4 x i8] zeroinitializer, align 8

; TLS variables must be expanded out by ExpandTls.
; CHECK-NEXT: Variable tls_var has disallowed "thread_local" attribute
@tls_var = thread_local global [4 x i8] zeroinitializer


; Function attributes

; CHECK-NEXT: Function func_with_attrs has disallowed attributes: noreturn nounwind
define void @func_with_attrs() noreturn nounwind {
  ret void
}

; CHECK-NEXT: Function func_with_arg_attrs has disallowed attributes: inreg zeroext
define void @func_with_arg_attrs(i32 inreg zeroext) {
  ret void
}

; CHECK-NEXT: Function func_with_section has disallowed "section" attribute
define void @func_with_section() section ".some_section" {
  ret void
}

; TODO(mseaborn): PNaCl programs don't know what alignment is
; reasonable for a function, so we should disallow this.
; CHECK-NOT: func_with_alignment
define void @func_with_alignment() align 1 {
  ret void
}

; CHECK-NEXT: Function func_with_gc has disallowed "gc" attribute
define void @func_with_gc() gc "my_gc_func" {
  ret void
}

; CHECK-NOT: disallowed
; If another check is added, there should be a check-not in between each check
