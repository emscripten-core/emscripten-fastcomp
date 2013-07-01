; RUN: pnacl-abicheck < %s | FileCheck %s

; This intrinsic is declared with the wrong type, using i32* arguments
; instead of i8*.  Check that the ABI verifier rejects this.  This
; must be tested in a separate .ll file from the correct intrinsic
; declarations.

declare void @llvm.memcpy.p0i8.p0i8.i32(i32* %dest, i32* %src,
                                        i32 %len, i32 %align, i1 %isvolatile)
; CHECK: Function llvm.memcpy.p0i8.p0i8.i32 is a disallowed LLVM intrinsic
