; RUN: opt < %s -nacl-global-cleanup -S | FileCheck %s
; RUN: opt < %s -nacl-global-cleanup -S | FileCheck -check-prefix=GV %s

@a = global i8 42

@llvm.compiler.used = appending global [1 x i8*] [i8* @a], section "llvm.metadata"
; GV-NOT: llvm.compiler.used

@llvm.used = appending global [1 x i8*] [i8* @a], section "llvm.metadata"
; The used list remains unchanged.
; CHECK: llvm.used

@extern_weak_const = extern_weak constant i32
@extern_weak_gv = extern_weak global i32

; GV-NOT: @extern_weak_const
; GV-NOT: @extern_weak_gv

; CHECK: @weak_gv = internal global
@weak_gv = weak global i32 0

; CHECK: define void @_start
define void @_start() {
  ret void
}

define i32* @ewgv() {
; CHECK: %bc = getelementptr i8, i8* null, i32 0
  %bc = getelementptr i8, i8* bitcast (i32* @extern_weak_gv to i8*), i32 0
; CHECK: ret i32* null
  ret i32* @extern_weak_gv
}

define i32* @ewc() {
; CHECK: %bc = getelementptr i8, i8* null, i32 0
  %bc = getelementptr i8, i8* bitcast (i32* @extern_weak_const to i8*), i32 0
; CHECK: ret i32* null
  ret i32* @extern_weak_gv
}

; Make sure @weak_gv is actually used.
define i32* @wgv() {
; CHECK: ret i32* @weak_gv
  ret i32* @weak_gv
}

; GV-NOT: @extern_weak_func
declare extern_weak i32 @extern_weak_func()
; CHECK: @ewf
define i32 @ewf() {
; CHECK: %ret = call i32 null()
  %ret = call i32 @extern_weak_func()
  ret i32 %ret
}

; CHECK: define internal void @weak_func
define weak void @weak_func() {
  ret void
}
