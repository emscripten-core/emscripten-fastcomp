; Currently we do not define __{init,fini}_array_end as named aliases.
; RUN: opt < %s -nacl-expand-ctors -S | not grep __init_array_end
; RUN: opt < %s -nacl-expand-ctors -S | not grep __fini_array_end

; We expect this symbol to be removed:
; RUN: opt < %s -nacl-expand-ctors -S | not grep llvm.global_ctors

; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s

; If llvm.global_ctors is zeroinitializer, it should be treated the
; same as an empty array.

@llvm.global_ctors = appending global [0 x { i32, void ()* }] zeroinitializer

; CHECK: @__init_array_start = internal constant [0 x void ()*] zeroinitializer
; CHECK: @__fini_array_start = internal constant [0 x void ()*] zeroinitializer
