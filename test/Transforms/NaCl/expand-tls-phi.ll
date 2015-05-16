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
; CHECK: define i32* @get_tvar(i1 %cmp) {
; CHECK: entry:
; CHECK: %field = getelementptr %tls_struct, %tls_struct* %tls_struct, i32 -1, i32 0, i32 0
; CHECK: else:
; CHECK: return:
; CHECK: %result = phi i32* [ %field, %entry ], [ null, %else ]


; This tests that ExpandTls correctly handles a PHI node that contains
; the same TLS variable twice.  Using replaceAllUsesWith() is not
; correct on a PHI node when the new instruction has to be added to an
; incoming block.
define i32* @tls_phi_twice(i1 %arg) {
  br i1 %arg, label %iftrue, label %iffalse
iftrue:
  br label %exit
iffalse:
  br label %exit
exit:
  %result = phi i32* [ @tvar, %iftrue ], [ @tvar, %iffalse ]
  ret i32* %result
}
; CHECK: define i32* @tls_phi_twice(i1 %arg) {
; CHECK: iftrue:
; CHECK: %field{{.*}} = getelementptr %tls_struct, %tls_struct* %tls_struct{{.*}}, i32 -1, i32 0, i32 0
; CHECK: iffalse:
; CHECK: %field{{.*}} = getelementptr %tls_struct, %tls_struct* %tls_struct{{.*}}, i32 -1, i32 0, i32 0
; CHECK: exit:
; CHECK: %result = phi i32* [ %field{{.*}}, %iftrue ], [ %field{{.*}}, %iffalse ]


; In this corner case, ExpandTls must expand out @tvar only once,
; otherwise it will produce invalid IR.
define i32* @tls_phi_multiple_entry(i1 %arg) {
entry:
  br i1 %arg, label %done, label %done
done:
  %result = phi i32* [ @tvar, %entry ], [ @tvar, %entry ]
  ret i32* %result
}
; CHECK: define i32* @tls_phi_multiple_entry(i1 %arg) {
; CHECK: %result = phi i32* [ %field, %entry ], [ %field, %entry ]
