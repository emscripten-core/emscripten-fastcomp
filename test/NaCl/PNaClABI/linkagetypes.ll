; RUN: pnacl-abicheck < %s | FileCheck %s
; Test linkage types allowed by PNaCl ABI

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"


@gv_external = external global i8
@gv_private = private global i8 0
@gv_linker_private = linker_private global i32 0
; CHECK-NOT: disallowed
; CHECK: gv_linker_private has disallowed linkage type: linker_private
@gv_linker_private_weak = linker_private_weak global i32 0
; CHECK: gv_linker_private_weak has disallowed linkage type: linker_private_weak
@gv_internal = internal global i8 0
@gv_linkonce = linkonce global i8 0
; CHECK-NOT: disallowed
; CHECK: gv_linkonce has disallowed linkage type: linkonce
@gv_linkonce_odr = linkonce_odr global i8 0
; CHECK: gv_linkonce_odr has disallowed linkage type: linkonce_odr
@gv_linkonce_odr_auto_hide = linkonce_odr_auto_hide global i8 0
; CHECK: gv_linkonce_odr_auto_hide has disallowed linkage type: linkonce_odr_auto_hide
@gv_weak = weak global i8 0
; CHECK: gv_weak has disallowed linkage type: weak
@gv_weak_odr = weak_odr global i8 0
; CHECK: gv_weak_odr has disallowed linkage type: weak_odr
@gv_common = common global i8 0
; CHECK: gv_common has disallowed linkage type: common
@gv_appending = appending global [1 x i8] zeroinitializer
; CHECK: gv_appending has disallowed linkage type: appending
@gv_dllimport = dllimport global i8
; CHECK: gv_dllimport has disallowed linkage type: dllimport
@gv_dllexport = dllexport global i8 0
; CHECK: gv_dllexport has disallowed linkage type: dllexport
@gv_extern_weak = extern_weak global i8
; CHECK: gv_extern_weak has disallowed linkage type: extern_weak
@gv_avilable_externally = available_externally global i8 0
