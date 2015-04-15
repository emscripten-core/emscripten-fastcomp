; Tests calls to intrinsics with bad type signatures.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | not pnacl-bcdis | FileCheck %s

; Error where return type should be i32.
declare void @llvm.nacl.setjmp(i8*)
; CHECK: Error({{.*}}): Intrinsic llvm.nacl.setjmp expects return type i32. Found: void

; Error where type of 2nd parameter is wrong (should be i32).
declare i32 @llvm.nacl.atomic.load.i32(i32*, i64)
; CHECK: Error({{.*}}): Intrinsic llvm.nacl.atomic.load.i32 expects i32 for argument 2. Found: i64

; Error where too many arguments are specified.
declare void @llvm.nacl.atomic.store.i64(i64, i64*, i32, i32)
; CHECK: Error({{.*}}): Intrinsic llvm.nacl.atomic.store.i64 expects 3 arguments. Found: 4
