; Currently we do not define __{init,fini}_array_end as named aliases.
; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s -check-prefix=NO_CTORS
; NO_CTORS-NOT: __init_array_end
; NO_CTORS-NOT: __fini_array_end

; We expect this symbol to be removed:
; RUN: opt < %s -nacl-expand-ctors -S | not grep llvm.global_ctors

; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s

; If llvm.global_ctors is zeroinitializer, it should be treated the
; same as an empty array.

@llvm.global_ctors = appending global [0 x { i32, void ()* }] zeroinitializer

; CHECK: @__init_array_start = internal constant [0 x void ()*] zeroinitializer
; CHECK: @__fini_array_start = internal constant [0 x void ()*] zeroinitializer
