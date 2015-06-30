; RUN: opt < %s -simplify-allocas -S | FileCheck %s

target datalayout = "p:32:32:32"

%struct = type { i32, i32 }

declare void @receive_alloca(%struct* %ptr)
declare void @receive_vector_alloca(<4 x i32>* %ptr)

define void @alloca_fixed() {
  %buf = alloca %struct, align 128
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_fixed() {
; CHECK-NEXT:    %buf = alloca i8, i32 8, align 128
; CHECK-NEXT:    %buf.bc = bitcast i8* %buf to %struct*
; CHECK-NEXT:    call void @receive_alloca(%struct* %buf.bc)

; When the size passed to alloca is a constant, it should be a
; constant in the output too.
define void @alloca_fixed_array() {
  %buf = alloca %struct, i32 100
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_fixed_array() {
; CHECK-NEXT:    %buf = alloca i8, i32 800, align 8
; CHECK-NEXT:    %buf.bc = bitcast i8* %buf to %struct*
; CHECK-NEXT:    call void @receive_alloca(%struct* %buf.bc)

define void @alloca_fixed_vector() {
  %buf = alloca <4 x i32>, align 128
  call void @receive_vector_alloca(<4 x i32>* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_fixed_vector() {
; CHECK-NEXT: %buf = alloca i8, i32 16, align 128
; CHECK-NEXT: %buf.bc = bitcast i8* %buf to <4 x i32>*
; CHECK-NEXT: call void @receive_vector_alloca(<4 x i32>* %buf.bc)

define void @alloca_variable(i32 %size) {
  %buf = alloca %struct, i32 %size
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_variable(i32 %size) {
; CHECK-NEXT:    %buf.alloca_mul = mul i32 8, %size
; CHECK-NEXT:    %buf = alloca i8, i32 %buf.alloca_mul
; CHECK-NEXT:    %buf.bc = bitcast i8* %buf to %struct*
; CHECK-NEXT:    call void @receive_alloca(%struct* %buf.bc)

define void @alloca_alignment_i32() {
  %buf = alloca i32
  ret void
}
; CHECK-LABEL: void @alloca_alignment_i32() {
; CHECK-NEXT:    alloca i8, i32 4, align 4

define void @alloca_alignment_double() {
  %buf = alloca double
  ret void
}
; CHECK-LABEL: void @alloca_alignment_double() {
; CHECK-NEXT:    alloca i8, i32 8, align 8

define void @alloca_lower_alignment() {
  %buf = alloca i32, align 1
  ret void
}
; CHECK-LABEL: void @alloca_lower_alignment() {
; CHECK-NEXT:    alloca i8, i32 4, align 1

define void @alloca_array_trunc() {
  %a = alloca i32, i64 1024
  unreachable
}
; CHECK-LABEL: define void @alloca_array_trunc()
; CHECK-NEXT:    %a = alloca i8, i32 4096

define void @alloca_array_zext() {
  %a = alloca i32, i8 128
  unreachable
}
; CHECK-LABEL: define void @alloca_array_zext()
; CHECK-NEXT:    %a = alloca i8, i32 512

define void @dyn_alloca_array_trunc(i64 %a) {
  %b = alloca i32, i64 %a
  unreachable
}
; CHECK-LABEL: define void @dyn_alloca_array_trunc(i64 %a)
; CHECK-NEXT:    trunc i64 %a to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    alloca i8, i32

define void @dyn_alloca_array_zext(i8 %a) {
  %b = alloca i32, i8 %a
  unreachable
}
; CHECK-LABEL: define void @dyn_alloca_array_zext(i8 %a)
; CHECK-NEXT:    zext i8 %a to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    alloca i8, i32

define void @dyn_inst_alloca_array(i32 %a) {
  %b = add i32 1, %a
  %c = alloca i32, i32 %b
  unreachable
}
; CHECK-LABEL: define void @dyn_inst_alloca_array(i32 %a)
; CHECK-NEXT:    %b = add i32 1, %a
; CHECK-NEXT:    mul i32 4, %b
; CHECK-NEXT:    %c = alloca i8, i32

define void @dyn_inst_alloca_array_trunc(i64 %a) {
  %b = add i64 1, %a
  %c = alloca i32, i64 %b
  unreachable
}
; CHECK-LABEL: define void @dyn_inst_alloca_array_trunc(i64 %a)
; CHECK-NEXT:    %b = add i64 1, %a
; CHECK-NEXT:    trunc i64 %b to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    %c = alloca i8, i32

define void @dyn_inst_alloca_array_zext(i8 %a) {
  %b = add i8 1, %a
  %c = alloca i32, i8 %b
  unreachable
}
; CHECK-LABEL: define void @dyn_inst_alloca_array_zext(i8 %a)
; CHECK-NEXT:    %b = add i8 1, %a
; CHECK-NEXT:    zext i8 %b to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    %c = alloca i8, i32

declare void @llvm.dbg.declare(metadata, metadata, metadata)
define void @debug_declare() {
  %var = alloca i32
  call void @llvm.dbg.declare(metadata i32* %var, metadata !12, metadata !13), !dbg !14
  unreachable
}
; Ensure that the first arg to dbg.declare points to the alloca, not the bitcast
; CHECK-LABEL: define void @debug_declare
; CHECK-NEXT: %var = alloca i8, i32 4
; CHECK: call void @llvm.dbg.declare(metadata i8* %var, metadata !12, metadata !13), !dbg !14

define void @debug_declare_morecasts() {
  %var = alloca i32, i32 2, align 8
  %other_bc = bitcast i32* %var to i64*
  %other_bc2 = bitcast i64* %other_bc to i16*
  call void @llvm.dbg.declare(metadata i16* %other_bc2, metadata !15, metadata !13), !dbg !16
  unreachable
}
; Ensure that the first arg to dbg.declare points to the alloca, not bitcasts
; CHECK-LABEL: define void @debug_declare_morecasts
; CHECK-NEXT: %var = alloca i8, i32 8, align 8
; CHECK: call void @llvm.dbg.declare(metadata i8* %var, metadata !15, metadata !13), !dbg !16

define void @debug_declare_inttoptr() {
  %var = alloca i32, i32 2, align 8
  %i = ptrtoint i32* %var to i32
  %p = inttoptr i32 %i to i8*
  call void @llvm.dbg.declare(metadata i8* %p, metadata !15, metadata !13), !dbg !16
  unreachable
}
; Ensure that we can look through ptrtoint/inttoptr
; CHECK-LABEL: define void @debug_declare_inttoptr
; CHECK-NEXT: alloca i8, i32 8, align 8
; CHECK: call void @llvm.dbg.declare(metadata i8* %var, metadata !15, metadata !13), !dbg !16

declare i8* @foo()
define void @debug_declare_noalloca() {
  %call = tail call i8* @foo()
  %config_.i.i = getelementptr inbounds i8, i8* %call, i32 104, !dbg !16
  %bc = bitcast i8* %config_.i.i to i16*, !dbg !16
  tail call void @llvm.dbg.declare(metadata i16* %bc, metadata !15, metadata !13), !dbg !16
  unreachable
}
; Don't modify dbg.declares which don't ultimately point to an alloca.
; CHECK-LABEL: define void @debug_declare_noalloca()
; CHECK: call void @llvm.dbg.declare(metadata i16* %bc, metadata !15, metadata !13), !dbg !16

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!9, !10}
!llvm.ident = !{!11}

; CHECK: !4 = !MDSubprogram(name: "debug_declare", scope: !1, file: !1, line: 1, type: !5, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: false, function: void ()* @debug_declare, variables: !2)

!0 = !MDCompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 3.7.0 (trunk 235150) (llvm/trunk 235152)", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, retainedTypes: !2, subprograms: !3, globals: !2, imports: !2)
!1 = !MDFile(filename: "foo.c", directory: "/s/llvm/cmakebuild")
!2 = !{}
!3 = !{!4, !8}
!4 = !MDSubprogram(name: "debug_declare", scope: !1, file: !1, line: 1, type: !5, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: false, function: void ()* @debug_declare, variables: !2)
!5 = !MDSubroutineType(types: !6)
!6 = !{null, !7}
!7 = !MDBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!8 = !MDSubprogram(name: "debug_declare_morecasts", scope: !1, file: !1, line: 8, type: !5, isLocal: false, isDefinition: true, scopeLine: 8, flags: DIFlagPrototyped, isOptimized: false, function: void ()* @debug_declare_morecasts, variables: !2)
!9 = !{i32 2, !"Dwarf Version", i32 4}
!10 = !{i32 2, !"Debug Info Version", i32 3}
!11 = !{!"clang version 3.7.0 (trunk 235150) (llvm/trunk 235152)"}
!12 = !MDLocalVariable(tag: DW_TAG_arg_variable, name: "val", arg: 1, scope: !4, file: !1, line: 1, type: !7)
!13 = !MDExpression()
!14 = !MDLocation(line: 1, column: 24, scope: !4)
!15 = !MDLocalVariable(tag: DW_TAG_arg_variable, name: "var", arg: 1, scope: !8, file: !1, line: 9, type: !7)
!16 = !MDLocation(line: 9, column: 24, scope: !8)
