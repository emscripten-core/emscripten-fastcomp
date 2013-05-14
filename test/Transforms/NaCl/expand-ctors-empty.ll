; Currently we do not define __{init,fini}_array_end as named aliases.
; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s -check-prefix=NO_CTORS
; NO_CTORS-NOT: __init_array_end
; NO_CTORS-NOT: __fini_array_end

; RUN: opt < %s -nacl-expand-ctors -S | FileCheck %s

; If llvm.global_ctors is not present, it is treated as if it is an
; empty array, and __{init,fini}_array_start are defined anyway.

; CHECK: @__init_array_start = internal constant [0 x void ()*] zeroinitializer
; CHECK: @__fini_array_start = internal constant [0 x void ()*] zeroinitializer
