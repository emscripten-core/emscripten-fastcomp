; RUN: pnacl-abicheck -pnaclabi-allow-dev-intrinsics=0 < %s | FileCheck %s
; RUN: pnacl-abicheck -pnaclabi-allow-dev-intrinsics=0 \
; RUN:   -pnaclabi-allow-debug-metadata < %s | FileCheck %s --check-prefix=DBG
; RUN: pnacl-abicheck -pnaclabi-allow-dev-intrinsics=1 < %s | \
; RUN:   FileCheck %s --check-prefix=DEV

; Test that only white-listed intrinsics are allowed.

; ===================================
; Some disallowed "Dev" intrinsics.
; CHECK: Function llvm.dbg.value is a disallowed LLVM intrinsic
; DBG-NOT: Function llvm.dbg.value is a disallowed LLVM intrinsic
; DEV-NOT: Function llvm.dbg.value is a disallowed LLVM intrinsic
declare void @llvm.dbg.value(metadata, i64, metadata)

; CHECK: Function llvm.frameaddress is a disallowed LLVM intrinsic
; DEV-NOT: Function llvm.frameaddress is a disallowed LLVM intrinsic
declare i8* @llvm.frameaddress(i32 %level)

; CHECK: Function llvm.returnaddress is a disallowed LLVM intrinsic
; DEV-NOT: Function llvm.returnaddress is a disallowed LLVM intrinsic
declare i8* @llvm.returnaddress(i32 %level)

; ===================================
; Always allowed intrinsics.
; CHECK-NOT: Function llvm.lifetime.start is a disallowed LLVM intrinsic
; DBG-NOT: Function llvm.lifetime.start is a disallowed LLVM intrinsic
; DEV-NOT: Function llvm.lifetime.start is a disallowed LLVM intrinsic
declare void @llvm.lifetime.start(i64, i8* nocapture)

; CHECK-NOT: Function llvm.lifetime.start is a disallowed LLVM intrinsic
declare void @llvm.lifetime.end(i64, i8* nocapture)
; CHECK-NOT: Function llvm.memcpy.p0i8.p0i8.i32 is a disallowed LLVM intrinsic
declare void @llvm.memcpy.p0i8.p0i8.i32(i8* %dest, i8* %src,
                                        i32 %len, i32 %align, i1 %isvolatile)
; CHECK-NOT: Function llvm.memcpy.p0i8.p0i8.i64 is a disallowed LLVM intrinsic
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* %dest, i8* %src,
                                        i64 %len, i32 %align, i1 %isvolatile)
; CHECK-NOT: Function llvm.nacl.read.tp is a disallowed LLVM intrinsic
declare i8* @llvm.nacl.read.tp()

; ===================================
; Always disallowed intrinsics.
; CHECK: Function llvm.adjust.trampoline is a disallowed LLVM intrinsic
; DBG: Function llvm.adjust.trampoline is a disallowed LLVM intrinsic
; DEV: Function llvm.adjust.trampoline is a disallowed LLVM intrinsic
declare i8* @llvm.adjust.trampoline(i8*)

; CHECK: Function llvm.init.trampoline is a disallowed LLVM intrinsic
; DBG: Function llvm.init.trampoline is a disallowed LLVM intrinsic
; DEV: Function llvm.init.trampoline is a disallowed LLVM intrinsic
declare void @llvm.init.trampoline(i8*, i8*, i8*)

; CHECK: Function llvm.x86.aesni.aeskeygenassist is a disallowed LLVM intrinsic
; DBG: Function llvm.x86.aesni.aeskeygenassist is a disallowed LLVM intrinsic
; DEV: Function llvm.x86.aesni.aeskeygenassist is a disallowed LLVM intrinsic
declare <2 x i64> @llvm.x86.aesni.aeskeygenassist(<2 x i64>, i8)

; CHECK: Function llvm.va_copy is a disallowed LLVM intrinsic
; DBG: Function llvm.va_copy is a disallowed LLVM intrinsic
; DEV: Function llvm.va_copy is a disallowed LLVM intrinsic
declare void @llvm.va_copy(i8*, i8*)
