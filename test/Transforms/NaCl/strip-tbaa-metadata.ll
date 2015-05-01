; RUN: opt -S -strip-metadata %s | FileCheck %s

; Test that !tbaa is removed from loads/stores.
; CHECK: @foo
; CHECK-NOT: !tbaa
define double @foo(i32* nocapture %ptr1, double* nocapture %ptr2) nounwind readonly {
  store i32 99999, i32* %ptr1, align 1, !tbaa !0
  %1 = load double, double* %ptr2, align 8, !tbaa !3
  ret double %1
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture, i64, i32, i1) nounwind

; Test that !tbaa is removed from calls.
; CHECK: @bar
; CHECK-NOT: !tbaa
define void @bar(i8* nocapture %p, i8* nocapture %q,
       i8* nocapture %s) nounwind {
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %p, i8* %q,
                                            i64 16, i32 1, i1 false), !tbaa !4
  store i8 2, i8* %s, align 1, !tbaa !5
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %q, i8* %p,
                                            i64 16, i32 1, i1 false), !tbaa !4
; CHECK ret void
  ret void
}

; Test that the metadata nodes aren't left over.
; CHECK-NOT: !0 =

!0 = !{!"int", !1}
!1 = !{!"omnipotent char", !2}
!2 = !{!"Simple C/C++ TBAA"}
!3 = !{!"double", !1}
!4 = !{!"A", !1}
!5 = !{!"B", !1}
