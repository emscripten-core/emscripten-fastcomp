; RUN: not pnacl-abicheck < %s | FileCheck %s
; Test linkage types allowed by PNaCl ABI

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"


@gv_internal = internal global [1 x i8] c"x"
; CHECK-NOT: disallowed

@gv_private = private global [1 x i8] c"x"
; CHECK: Variable gv_private has disallowed linkage type: private
@gv_linkonce = linkonce global [1 x i8] c"x"
; CHECK: gv_linkonce has disallowed linkage type: linkonce
@gv_linkonce_odr = linkonce_odr global [1 x i8] c"x"
; CHECK: gv_linkonce_odr has disallowed linkage type: linkonce_odr
@gv_weak = weak global [1 x i8] c"x"
; CHECK: gv_weak has disallowed linkage type: weak
@gv_weak_odr = weak_odr global [1 x i8] c"x"
; CHECK: gv_weak_odr has disallowed linkage type: weak_odr
@gv_common = common global [1 x i8] c"x"
; CHECK: gv_common has disallowed linkage type: common
@gv_appending = appending global [1 x i8] zeroinitializer
; CHECK: gv_appending has disallowed linkage type: appending
@gv_dllimport = external dllimport global [1 x i8]
; CHECK: gv_dllimport is not a valid external symbol (disallowed)
@gv_dllexport = dllexport global [1 x i8] c"x"
; CHECK: gv_dllexport is not a valid external symbol (disallowed)
@gv_extern_weak = extern_weak global [1 x i8]
; CHECK: gv_extern_weak has disallowed linkage type: extern_weak
@gv_available_externally = available_externally global [1 x i8] c"x"
; CHECK: gv_available_externally has disallowed linkage type: available_externally


; CHECK-NOT: disallowed
; CHECK-NOT: internal_func
; internal linkage is allowed, and should not appear in error output.
define internal void @internal_func() {
  ret void
}

; CHECK: Function private_func has disallowed linkage type: private
define private void @private_func() {
  ret void
}
; CHECK: Function external_func is declared but not defined (disallowed)
declare external void @external_func()
; CHECK: linkonce_func has disallowed linkage type: linkonce
define linkonce void @linkonce_func() {
  ret void
}
; CHECK-NEXT: linkonce_odr_func has disallowed linkage type: linkonce_odr
define linkonce_odr void @linkonce_odr_func() {
  ret void
}
; CHECK-NEXT: weak_func has disallowed linkage type: weak
define weak void @weak_func() {
  ret void
}
; CHECK-NEXT: weak_odr_func has disallowed linkage type: weak_odr
define weak_odr void @weak_odr_func() {
  ret void
}
; CHECK-NEXT: dllimport_func is declared but not defined (disallowed)
; CHECK-NEXT: dllimport_func is not a valid external symbol (disallowed)
declare dllimport void @dllimport_func()
; CHECK-NEXT: dllexport_func is not a valid external symbol (disallowed)
define dllexport void @dllexport_func() {
  ret void
}
; CHECK-NEXT: Function extern_weak_func is declared but not defined (disallowed)
; CHECK-NEXT: Function extern_weak_func has disallowed linkage type: extern_weak
declare extern_weak void @extern_weak_func()

; CHECK-NEXT: Function avail_ext_func has disallowed linkage type: available_externally
define available_externally void @avail_ext_func() {
  ret void
}
