; RUN: opt -S -strip-metadata %s | FileCheck %s --check-prefix=STRIPMETA
; RUN: opt -S -strip-module-flags %s | FileCheck %s --check-prefix=STRIPMODF
; RUN: opt -S -strip-metadata -strip-module-flags -strip-debug %s | FileCheck %s --check-prefix=STRIPALL

define i32 @foo(i32 %c) {
; STRIPMETA: @foo
; STRIPMETA-NEXT: call void @llvm.dbg{{.*}}, !dbg
; STRIPMETA-NEXT: ret{{.*}}, !dbg
; STRIPMODF: @foo
; STRIPMODF-NEXT: call void @llvm.dbg{{.*}}, !dbg
; STRIPMODF-NEXT: ret{{.*}}, !dbg
; STRIPALL: @foo
; STRIPALL-NOT: !dbg
  tail call void @llvm.dbg.value(metadata !{i32 %c}, i64 0, metadata !9), !dbg !10
  ret i32 %c, !dbg !11
}

; STRIPMETA: @llvm.dbg.value
; STRIPMODF: @llvm.dbg.value
; STRIPALL: ret i32
; STRIPALL-NOT: @llvm.dbg.value
declare void @llvm.dbg.value(metadata, i64, metadata) #1

; STRIPMETA-NOT: MadeUpMetadata
; STRIPMODF-NOT: MadeUpMetadata
!MadeUpMetadata = !{}

; STRIPMETA: !llvm.dbg.cu
; STRIPMODF: !llvm.dbg.cu
!llvm.dbg.cu = !{!0}

; STRIPMETA: llvm.module.flags
; STRIPMODF-NOT: llvm.module.flags
; STRIPALL-NOT: llvm.module.flags
!llvm.module.flags = !{!12,!13}

; STRIPMETA: !0 =
; STRIPMODF: !0 =
!0 = metadata !{i32 786449, i32 0, i32 12, metadata !"test.c", metadata !"/tmp", metadata !"clang version 3.3 (trunk 176732) (llvm/trunk 176733)", i1 true, i1 true, metadata !"", i32 0, metadata !1, metadata !1, metadata !2, metadata !1, metadata !""} ; [ DW_TAG_compile_unit ] [/tmp/test.c] [DW_LANG_C99]
!1 = metadata !{i32 0}
!2 = metadata !{metadata !3}
!3 = metadata !{i32 786478, i32 0, metadata !4, metadata !"foo", metadata !"foo", metadata !"", metadata !4, i32 1, metadata !5, i1 false, i1 true, i32 0, i32 0, null, i32 256, i1 true, i32 (i32)* @foo, null, null, metadata !8, i32 1} ; [ DW_TAG_subprogram ] [line 1] [def] [foo]
!4 = metadata !{i32 786473, metadata !"test.c", metadata !"/tmp", null} ; [ DW_TAG_file_type ]
!5 = metadata !{i32 786453, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i64 0, i32 0, null, metadata !6, i32 0, i32 0} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!6 = metadata !{metadata !7, metadata !7}
!7 = metadata !{i32 786468, null, metadata !"int", null, i32 0, i64 32, i64 32, i64 0, i32 0, i32 5} ; [ DW_TAG_base_type ] [int] [line 0, size 32, align 32, offset 0, enc DW_ATE_signed]
!8 = metadata !{metadata !9}
!9 = metadata !{i32 786689, metadata !3, metadata !"c", metadata !4, i32 16777217, metadata !7, i32 0, i32 0} ; [ DW_TAG_arg_variable ] [c] [line 1]
!10 = metadata !{i32 1, i32 0, metadata !3, null}
!11 = metadata !{i32 2, i32 0, metadata !3, null}
; STRIPMETA: Linker Options
; STRIPMODF-NOT: Linker Options
; STRIPALL-NOT: Linker Options
!12 = metadata !{ i32 6, metadata !"Linker Options",
     metadata !{
        metadata !{ metadata !"-lz" },
        metadata !{ metadata !"-framework", metadata !"Cocoa" } } }
; STRIPMETA: Debug Info Version
; STRIPMODF-NOT: Debug Info Version
; STRIPALL-NOT: Debug Info Version
!13 = metadata !{i32 1, metadata !"Debug Info Version", i32 2}
