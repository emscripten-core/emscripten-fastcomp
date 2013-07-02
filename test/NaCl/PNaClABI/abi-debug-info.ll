; RUN: pnacl-abicheck -pnaclabi-allow-dev-intrinsics=0 < %s | FileCheck %s
; RUN: pnacl-abicheck -pnaclabi-allow-dev-intrinsics=0 \
; RUN:   -pnaclabi-allow-debug-metadata < %s | FileCheck %s --check-prefix=DBG
; RUN: pnacl-abicheck -pnaclabi-allow-dev-intrinsics=1 < %s | \
; RUN:   FileCheck %s --check-prefix=DBG


; DBG-NOT: disallowed


declare void @llvm.dbg.declare(metadata, metadata)
declare void @llvm.dbg.value(metadata, i64, metadata)

; CHECK: Function llvm.dbg.declare is a disallowed LLVM intrinsic
; CHECK: Function llvm.dbg.value is a disallowed LLVM intrinsic


define internal void @debug_declare(i32 %val) {
  ; We normally expect llvm.dbg.declare to be used on an alloca.
  %var = alloca [4 x i8]
  tail call void @llvm.dbg.declare(metadata !{[4 x i8]* %var}, metadata !{})
  tail call void @llvm.dbg.declare(metadata !{i32 %val}, metadata !{})
  ret void
}

define internal void @debug_value(i32 %ptr_as_int, i32 %val) {
  %ptr = inttoptr i32 %ptr_as_int to i8*
  tail call void @llvm.dbg.value(metadata !{i8* %ptr}, i64 2, metadata !{})
  tail call void @llvm.dbg.value(metadata !{i32 %val}, i64 1, metadata !{})
  ret void
}

; FileCheck gives an error if its input file is empty, so ensure that
; the output of pnacl-abicheck is non-empty by generating at least one
; error.
declare void @bad_func(ppc_fp128 %bad_arg)
; DBG: Function bad_func has disallowed type: void (ppc_fp128)
