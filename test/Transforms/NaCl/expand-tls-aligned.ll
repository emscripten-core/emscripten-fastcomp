; RUN: opt < %s -nacl-expand-tls -S | FileCheck %s

target datalayout = "p:32:32:32"


@var = global i32 123

; Put this first to check that the pass handles BSS variables last.
@bss_tvar_aligned = thread_local global i32 0, align 64

@tvar1 = thread_local global i16 234
; Test a pointer to check we are getting the right pointer size.
@tvar2 = thread_local global i32* @var
@tvar_aligned = thread_local global i8 99, align 32


; CHECK: %tls_init_template = type <{ i16, [2 x i8], i32*, [24 x i8], i8 }>
; CHECK: %tls_struct = type <{ %tls_init_template, %tls_bss_template }>

; This struct type must be "packed" because the 31 byte padding here
; is followed by an i32.
; CHECK: %tls_bss_template = type <{ [31 x i8], i32, [60 x i8] }>

; CHECK: @__tls_template_start = internal constant %tls_init_template <{ i16 234, [2 x i8] zeroinitializer, i32* @var, [24 x i8] zeroinitializer, i8 99 }>

; CHECK: @__tls_template_alignment = internal constant i32 64


; Create references to __tls_template_* to keep these live, otherwise
; the definition of %tls_struct (which we check for above) is removed
; from the output.

@__tls_template_tdata_end = external global i8
@__tls_template_end = external global i8

define i8* @get_tls_template_tdata_end() {
  ret i8* @__tls_template_tdata_end
}

define i8* @get_tls_template_end() {
  ret i8* @__tls_template_end
}
