; RUN: opt < %s -nacl-expand-tls -S | FileCheck %s

@tvar = thread_local global i32 0

define i32 @get_tvar() {
  ret i32 ptrtoint (i32* @tvar to i32)
}
; CHECK: %tls_raw = call i8* @llvm.nacl.read.tp()
; CHECK: %tls_struct = bitcast i8* %tls_raw to %tls_struct*
; CHECK: %field = getelementptr %tls_struct, %tls_struct* %tls_struct, i32 -1, i32 1, i32 0
; CHECK: %expanded = ptrtoint i32* %field to i32
; CHECK: ret i32 %expanded
