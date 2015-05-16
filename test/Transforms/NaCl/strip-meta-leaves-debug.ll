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
  tail call void @llvm.dbg.value(metadata i32 %c, i64 0, metadata !9, metadata !13), !dbg !14
  ret i32 %c, !dbg !15
}

; STRIPMETA: @llvm.dbg.value
; STRIPMODF: @llvm.dbg.value
; STRIPALL: ret i32
; STRIPALL-NOT: @llvm.dbg.value
declare void @llvm.dbg.value(metadata, i64, metadata, metadata)

; STRIPMETA-NOT: MadeUpMetadata
; STRIPMODF-NOT: MadeUpMetadata
!MadeUpMetadata = !{}

; STRIPMETA: !llvm.dbg.cu
; STRIPMODF: !llvm.dbg.cu
!llvm.dbg.cu = !{!0}

; STRIPMETA: llvm.module.flags
; STRIPMODF-NOT: llvm.module.flags
; STRIPALL-NOT: llvm.module.flags
!llvm.module.flags = !{!10, !11, !24}

; STRIPMETA: !0 =
; STRIPMODF: !0 =


; STRIPMETA: Debug Info Version
; STRIPMODF-NOT: Debug Info Version
; STRIPALL-NOT: Debug Info Version
!11 = !{i32 2, !"Debug Info Version", i32 3}

; STRIPMETA: Linker Options
; STRIPMODF-NOT: Linker Options
; STRIPALL-NOT: Linker Options
!24 = !{i32 6, !"Linker Options", !{!{!"-lz"}, !{!"-framework", !"Cocoa"}, !{!"-lmath"}}}


!llvm.ident = !{!12}

!0 = !MDCompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 3.7.0 (trunk 235150) (llvm/trunk 235152)", isOptimized: true, runtimeVersion: 0, emissionKind: 1, enums: !2, retainedTypes: !2, subprograms: !3, globals: !2, imports: !2)
!1 = !MDFile(filename: "foo.c", directory: "/s/llvm/cmakebuild")
!2 = !{}
!3 = !{!4}
!4 = !MDSubprogram(name: "foo", scope: !1, file: !1, line: 1, type: !5, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: true, function: i32 (i32)* @foo, variables: !8)
!5 = !MDSubroutineType(types: !6)
!6 = !{!7, !7}
!7 = !MDBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!8 = !{!9}
!9 = !MDLocalVariable(tag: DW_TAG_arg_variable, name: "c", arg: 1, scope: !4, file: !1, line: 1, type: !7)
!10 = !{i32 2, !"Dwarf Version", i32 4}
!12 = !{!"clang version 3.7.0 (trunk 235150) (llvm/trunk 235152)"}
!13 = !MDExpression()
!14 = !MDLocation(line: 1, column: 13, scope: !4)
!15 = !MDLocation(line: 2, column: 3, scope: !4)

