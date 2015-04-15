; RUN: opt %s -nacl-expand-ctors -S | FileCheck %s -check-prefix=NO_CTORS
; NO_CTORS-NOT: __init_array_end
; NO_CTORS-NOT: __fini_array_end
; NO_CTORS-NOT: llvm.global_ctors

; RUN: opt %s -nacl-expand-ctors -S | FileCheck %s

; Check that the pass works when the initializer is "[]", which gets
; converted into "undef" by the reader.
@llvm.global_ctors = appending global [0 x { i32, void ()* }] []

; CHECK: @__init_array_start = internal constant [0 x void ()*] zeroinitializer
; CHECK: @__fini_array_start = internal constant [0 x void ()*] zeroinitializer
