; Test that intrinsic declarations are read back correctly.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD

declare i8* @llvm.stacksave()
declare void @llvm.stackrestore(i8*)

declare i8* @llvm.nacl.read.tp()
declare void @llvm.nacl.longjmp(i8*, i32)
declare void @llvm.nacl.setjmp(i8*)

declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture, i32, i32, i1)
declare void @llvm.memmove.p0i8.p0i8.i32(i8* nocapture, i8* nocapture, i32, i32, i1)
declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1)

declare i32 @llvm.nacl.atomic.load.i32(i32*, i32)
declare i64 @llvm.nacl.atomic.load.i64(i64*, i32)

declare i32 @llvm.nacl.atomic.cmpxchg.i32(i32*, i32, i32, i32, i32)
declare i64 @llvm.nacl.atomic.cmpxchg.i64(i64*, i64, i64, i32, i32)

declare void @llvm.nacl.atomic.store.i32(i32, i32*, i32)
declare void @llvm.nacl.atomic.store.i64(i64, i64*, i32)

declare i32 @llvm.nacl.atomic.rmw.i32(i32, i32*, i32, i32)
declare i64 @llvm.nacl.atomic.rmw.i64(i32, i64*, i64, i32)

declare i1 @llvm.nacl.atomic.is.lock.free(i32, i8*)


; TD: declare i8* @llvm.stacksave()
; TD: declare void @llvm.stackrestore(i8*)

; TD: declare i8* @llvm.nacl.read.tp()
; TD: declare void @llvm.nacl.longjmp(i8*, i32)
; TD: declare void @llvm.nacl.setjmp(i8*)

; TD: declare void @llvm.memcpy.p0i8.p0i8.i32(i8* {{.*}}, i8* {{.*}}, i32, i32, i1)
; TD: declare void @llvm.memmove.p0i8.p0i8.i32(i8* {{.*}}, i8* {{.*}}, i32, i32, i1)
; TD: declare void @llvm.memset.p0i8.i32(i8* {{.*}}, i8, i32, i32, i1)

; TD: declare i32 @llvm.nacl.atomic.load.i32(i32*, i32)
; TD: declare i64 @llvm.nacl.atomic.load.i64(i64*, i32)

; TD: declare i32 @llvm.nacl.atomic.cmpxchg.i32(i32*, i32, i32, i32, i32)
; TD: declare i64 @llvm.nacl.atomic.cmpxchg.i64(i64*, i64, i64, i32, i32)

; TD: declare void @llvm.nacl.atomic.store.i32(i32, i32*, i32)
; TD: declare void @llvm.nacl.atomic.store.i64(i64, i64*, i32)

; TD: declare i32 @llvm.nacl.atomic.rmw.i32(i32, i32*, i32, i32)
; TD: declare i64 @llvm.nacl.atomic.rmw.i64(i32, i64*, i64, i32)

; TD: declare i1 @llvm.nacl.atomic.is.lock.free(i32, i8*)
