; Currently we do not define __{init,fini}_array_end as named aliases.
; RUN: opt < %s -nacl-expand-ctors -S | not grep __init_array_end
; RUN: opt < %s -nacl-expand-ctors -S | not grep __fini_array_end

; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s

; If llvm.global_ctors is not present, it is treated as if it is an
; empty array, and __{init,fini}_array_start are defined anyway.

; CHECK: @__init_array_start = internal constant [0 x void ()*] zeroinitializer
; CHECK: @__fini_array_start = internal constant [0 x void ()*] zeroinitializer
