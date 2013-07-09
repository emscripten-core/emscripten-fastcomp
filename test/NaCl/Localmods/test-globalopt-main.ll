; RUN: opt < %s -globalopt -S | FileCheck %s

; Check that our LOCALMOD for the GlobalOpt optimization is working properly.
; The user code entry point is a function named main that has a single user:
; a call from _start.
; @globchar can be folded into an alloca inside @main, and the global can be
; deleted.

@globchar = internal global i8* null, align 8
; CHECK-NOT: @globchar = internal global

define internal i32 @main(i32 %argc, i8** %argv) {
  ; CHECK: @main(i32
entry:
  ; CHECK: %globchar = alloca i8*
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  %0 = load i8*** %argv.addr, align 8
  %arrayidx = getelementptr inbounds i8** %0, i64 0
  %1 = load i8** %arrayidx, align 8
  store i8* %1, i8** @globchar, align 8
  %2 = load i8** @globchar, align 8
  %arrayidx1 = getelementptr inbounds i8* %2, i64 1
  %3 = load i8* %arrayidx1, align 1
  call void @somefunc(i8 signext %3)
  ret i32 0
}

define i32 @_start(i32 %argc, i8** %argv) {
  %rv = call i32 @main(i32 %argc, i8** %argv)
  ret i32 %rv
}

declare void @somefunc(i8 signext)

