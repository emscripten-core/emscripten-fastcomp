; RUN: opt < %s -nacl-expand-tls -S | FileCheck %s


@tvar = thread_local global i32 123

define i32* @get_tvar(i1 %cmp) {
entry:
  br i1 %cmp, label %return, label %else

else:
  br label %return

return:
  %result = phi i32* [ @tvar, %entry ], [ null, %else ]
  ret i32* %result
}
; The TLS access gets pushed back into the PHI node's incoming block,
; which might be suboptimal but works in all cases.
; CHECK: entry:
; CHECK: %field = getelementptr %tls_struct* %tls_struct, i32 -1, i32 0, i32 0
; CHECK: else:
; CHECK: return:
; CHECK: %result = phi i32* [ %field, %entry ], [ null, %else ]
