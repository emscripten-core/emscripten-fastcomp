; We expect these symbol names to be removed:
; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s -check-prefix=NO_CTORS
; NO_CTORS-NOT: llvm.global.ctors
; NO_CTORS-NOT: __init_array_end
; NO_CTORS-NOT: __fini_array_end

; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s

@llvm.global_ctors = appending global [3 x { i32, void ()* }]
  [{ i32, void ()* } { i32 300, void ()* @init_func_A },
   { i32, void ()* } { i32 100, void ()* @init_func_B },
   { i32, void ()* } { i32 200, void ()* @init_func_C }]

@__init_array_start = extern_weak global [0 x void ()*]
@__init_array_end = extern_weak global [0 x void ()*]

; CHECK: @__init_array_start = internal constant [3 x void ()*] [void ()* @init_func_B, void ()* @init_func_C, void ()* @init_func_A]
; CHECK: @__fini_array_start = internal constant [0 x void ()*] zeroinitializer

define void @init_func_A() { ret void }
define void @init_func_B() { ret void }
define void @init_func_C() { ret void }

define [0 x void ()*]* @get_array_start() {
  ret [0 x void ()*]* @__init_array_start;
}
; CHECK: @get_array_start()
; CHECK: ret {{.*}} @__init_array_start

define [0 x void ()*]* @get_array_end() {
  ret [0 x void ()*]* @__init_array_end;
}

; @get_array_end() is converted to use a GetElementPtr that returns
; the end of the generated array:
; CHECK: @get_array_end()
; CHECK: ret {{.*}} bitcast ([3 x void ()*]* getelementptr inbounds ([3 x void ()*], [3 x void ()*]* @__init_array_start, i32 1)
