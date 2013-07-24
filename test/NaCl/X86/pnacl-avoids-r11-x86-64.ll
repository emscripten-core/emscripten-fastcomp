; The NACLON test verifies that %r11 and %r11d are not used except as
; part of the return sequence.
;
; RUN: pnacl-llc -O2 -mtriple=x86_64-none-nacl < %s | \
; RUN:     FileCheck %s --check-prefix=NACLON
;
; The NACLOFF test verifies that %r11 would normally be used if PNaCl
; weren't reserving r11 for its own uses, to be sure NACLON is a
; valid test.
;
; RUN: pnacl-llc -O2 -mtriple=x86_64-linux < %s | \
; RUN:     FileCheck %s --check-prefix=NACLOFF
;
; NACLON: RegisterPressure:
; NACLON-NOT: %r11
; NACLON: popq %r11
; NACLON: nacljmp %r11, %r15
;
; NACLOFF: RegisterPressure:
; NACLOFF: %r11
; NACLOFF: ret
; ModuleID = 'pnacl-avoids-r11-x86-64.c'
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"

@v1a = external global i32
@v1b = external global i32
@v2a = external global i32
@v2b = external global i32
@v3a = external global i32
@v3b = external global i32
@v4a = external global i32
@v4b = external global i32

; Function Attrs: nounwind
define void @RegisterPressure() #0 {
entry:
  %call = tail call i32 @GetValue() #2
  %call1 = tail call i32 @GetValue() #2
  %call2 = tail call i32 @GetValue() #2
  %call3 = tail call i32 @GetValue() #2
  %call4 = tail call i32 @GetValue() #2
  %call5 = tail call i32 @GetValue() #2
  %call6 = tail call i32 @GetValue() #2
  %call7 = tail call i32 @GetValue() #2
  %call8 = tail call i32 @GetValue() #2
  %call9 = tail call i32 @GetValue() #2
  %call10 = tail call i32 @GetValue() #2
  %call11 = tail call i32 @GetValue() #2
  %call12 = tail call i32 @GetValue() #2
  %call13 = tail call i32 @GetValue() #2
  %call14 = tail call i32 @GetValue() #2
  %call15 = tail call i32 @GetValue() #2
  tail call void @Use(i32 %call, i32 %call1, i32 %call2, i32 %call3, i32 %call4, i32 %call5, i32 %call6, i32 %call7, i32 %call8, i32 %call9, i32 %call10, i32 %call11, i32 %call12, i32 %call13, i32 %call14, i32 %call15) #2
  tail call void @Use(i32 %call, i32 %call1, i32 %call2, i32 %call3, i32 %call4, i32 %call5, i32 %call6, i32 %call7, i32 %call8, i32 %call9, i32 %call10, i32 %call11, i32 %call12, i32 %call13, i32 %call14, i32 %call15) #2
  %add = add nsw i32 %call1, %call
  %add16 = add nsw i32 %add, %call2
  %add17 = add nsw i32 %add16, %call3
  %add18 = add nsw i32 %add17, %call4
  %add19 = add nsw i32 %add18, %call5
  %add20 = add nsw i32 %add19, %call6
  %add21 = add nsw i32 %add20, %call7
  store volatile i32 %add21, i32* @v1a, align 4, !tbaa !0
  %add22 = add nsw i32 %call9, %call8
  %add23 = add nsw i32 %add22, %call10
  %add24 = add nsw i32 %add23, %call11
  %add25 = add nsw i32 %add24, %call12
  %add26 = add nsw i32 %add25, %call13
  %add27 = add nsw i32 %add26, %call14
  %add28 = add nsw i32 %add27, %call15
  store volatile i32 %add28, i32* @v1b, align 4, !tbaa !0
  %add32 = add nsw i32 %call8, %add17
  %add33 = add nsw i32 %add32, %call9
  %add34 = add nsw i32 %add33, %call10
  %add35 = add nsw i32 %add34, %call11
  store volatile i32 %add35, i32* @v2a, align 4, !tbaa !0
  %add36 = add nsw i32 %call5, %call4
  %add37 = add nsw i32 %add36, %call6
  %add38 = add nsw i32 %add37, %call7
  %add39 = add nsw i32 %add38, %call12
  %add40 = add nsw i32 %add39, %call13
  %add41 = add nsw i32 %add40, %call14
  %add42 = add nsw i32 %add41, %call15
  store volatile i32 %add42, i32* @v2b, align 4, !tbaa !0
  %add44 = add nsw i32 %call4, %add
  %add45 = add nsw i32 %add44, %call5
  %add46 = add nsw i32 %add45, %call8
  %add47 = add nsw i32 %add46, %call9
  %add48 = add nsw i32 %add47, %call12
  %add49 = add nsw i32 %add48, %call13
  store volatile i32 %add49, i32* @v3a, align 4, !tbaa !0
  %add50 = add nsw i32 %call3, %call2
  %add51 = add nsw i32 %add50, %call6
  %add52 = add nsw i32 %add51, %call7
  %add53 = add nsw i32 %add52, %call10
  %add54 = add nsw i32 %add53, %call11
  %add55 = add nsw i32 %add54, %call14
  %add56 = add nsw i32 %add55, %call15
  store volatile i32 %add56, i32* @v3b, align 4, !tbaa !0
  %add57 = add nsw i32 %call2, %call
  %add58 = add nsw i32 %add57, %call4
  %add59 = add nsw i32 %add58, %call6
  %add60 = add nsw i32 %add59, %call8
  %add61 = add nsw i32 %add60, %call10
  %add62 = add nsw i32 %add61, %call12
  %add63 = add nsw i32 %add62, %call14
  store volatile i32 %add63, i32* @v4a, align 4, !tbaa !0
  %add64 = add nsw i32 %call3, %call1
  %add65 = add nsw i32 %add64, %call5
  %add66 = add nsw i32 %add65, %call7
  %add67 = add nsw i32 %add66, %call9
  %add68 = add nsw i32 %add67, %call11
  %add69 = add nsw i32 %add68, %call13
  %add70 = add nsw i32 %add69, %call15
  store volatile i32 %add70, i32* @v4b, align 4, !tbaa !0
  tail call void @Use(i32 %call, i32 %call1, i32 %call2, i32 %call3, i32 %call4, i32 %call5, i32 %call6, i32 %call7, i32 %call8, i32 %call9, i32 %call10, i32 %call11, i32 %call12, i32 %call13, i32 %call14, i32 %call15) #2
  tail call void @Use(i32 %call, i32 %call1, i32 %call2, i32 %call3, i32 %call4, i32 %call5, i32 %call6, i32 %call7, i32 %call8, i32 %call9, i32 %call10, i32 %call11, i32 %call12, i32 %call13, i32 %call14, i32 %call15) #2
  ret void
}

declare i32 @GetValue() #1

declare void @Use(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32) #1

attributes #0 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind }

!0 = metadata !{metadata !"int", metadata !1}
!1 = metadata !{metadata !"omnipotent char", metadata !2}
!2 = metadata !{metadata !"Simple C/C++ TBAA"}
