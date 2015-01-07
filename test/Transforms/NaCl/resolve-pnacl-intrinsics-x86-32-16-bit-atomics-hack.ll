; RUN: opt < %s -resolve-pnacl-intrinsics -S -mtriple=i386-unknown-nacl | \
; RUN:   FileCheck %s -check-prefix=CLEANED
; RUN: opt < %s -resolve-pnacl-intrinsics -S -mtriple=i386-unknown-nacl | \
; RUN:   FileCheck %s

; CLEANED-NOT: call {{.*}} @llvm.nacl.atomic

; Supplement to resolve-pnacl-intrinsics.ll that tests the 16-bit hack
; for x86-32. All of the RMW cases are the same except for one
; operation.

; These declarations must be here because the function pass expects
; to find them. In real life they're inserted by the translator
; before the function pass runs.
declare i32 @setjmp(i8*)
declare void @longjmp(i8*, i32)

declare i16 @llvm.nacl.atomic.rmw.i16(i32, i16*, i16, i32)
declare i16 @llvm.nacl.atomic.cmpxchg.i16(i16*, i16, i16, i32, i32)

; CHECK: @test_fetch_and_add_i16
define i16 @test_fetch_and_add_i16(i16* %ptr, i16 %value) {
; CHECK-NEXT:  call void asm sideeffect "", "~{memory}"()
; CHECK-NEXT:  %uintptr = ptrtoint i16* %ptr to i32
; CHECK-NEXT:  %aligneduintptr = and i32 %uintptr, -4
; CHECK-NEXT:  %aligned32 = and i32 %uintptr, 3
; CHECK-NEXT:  %ptr32 = inttoptr i32 %aligneduintptr to i32*
; CHECK-NEXT:  %isaligned32 = icmp eq i32 %aligned32, 0
; CHECK-NEXT:  br i1 %isaligned32, label %atomic16aligned32, label %atomic16aligned16
;
; CHECK: atomic16successor:
; CHECK-NEXT:  %1 = phi i16 [ %truncval, %atomic16aligned32 ], [ %shval, %atomic16aligned16 ]
; CHECK-NEXT:  call void asm sideeffect "", "~{memory}"()
; CHECK-NEXT:  ret i16 %1
;
; CHECK: atomic16aligned32:
; CHECK-NEXT:  %loaded = load atomic i32* %ptr32 seq_cst, align 4
; CHECK-NEXT:  %truncval = trunc i32 %loaded to i16
; CHECK-NEXT:  %res = add i16 %truncval, %value
; CHECK-NEXT:  %mergeres = zext i16 %res to i32
; CHECK-NEXT:  %maskedloaded = and i32 %loaded, -65536
; CHECK-NEXT:  %finalres = or i32 %mergeres, %maskedloaded
; CHECK-NEXT:  %cmpxchg.results = cmpxchg i32* %ptr32, i32 %loaded, i32 %finalres seq_cst seq_cst
; CHECK-NEXT:  %success = extractvalue { i32, i1 } %cmpxchg.results, 1
; CHECK-NEXT:  br i1 %success, label %atomic16successor, label %atomic16aligned32
;
; CHECK: atomic16aligned16:
; CHECK-NEXT:  %loaded1 = load atomic i32* %ptr32 seq_cst, align 4
; CHECK-NEXT:  %lshr = lshr i32 %loaded1, 16
; CHECK-NEXT:  %shval = trunc i32 %lshr to i16
; CHECK-NEXT:  %res2 = add i16 %shval, %value
; CHECK-NEXT:  %zext = zext i16 %res2 to i32
; CHECK-NEXT:  %mergeres3 = shl i32 %zext, 16
; CHECK-NEXT:  %maskedloaded4 = and i32 %loaded1, 65535
; CHECK-NEXT:  %finalres5 = or i32 %mergeres3, %maskedloaded4
; CHECK-NEXT:  %cmpxchg.results6 = cmpxchg i32* %ptr32, i32 %loaded1, i32 %finalres5 seq_cst seq_cst
; CHECK-NEXT:  %success7 = extractvalue { i32, i1 } %cmpxchg.results6, 1
; CHECK-NEXT:  br i1 %success7, label %atomic16successor, label %atomic16aligned16
  %1 = call i16 @llvm.nacl.atomic.rmw.i16(i32 1, i16* %ptr, i16 %value, i32 6)
  ret i16 %1
}

; CHECK: @test_fetch_and_sub_i16
define i16 @test_fetch_and_sub_i16(i16* %ptr, i16 %value) {
  ; CHECK:   %res = sub i16 %truncval, %value
  ; CHECK:   %res2 = sub i16 %shval, %value
  %1 = call i16 @llvm.nacl.atomic.rmw.i16(i32 2, i16* %ptr, i16 %value, i32 6)
  ret i16 %1
}

; CHECK: @test_fetch_and_or_i16
define i16 @test_fetch_and_or_i16(i16* %ptr, i16 %value) {
  ; CHECK:   %res = or i16 %truncval, %value
  ; CHECK:   %res2 = or i16 %shval, %value
  %1 = call i16 @llvm.nacl.atomic.rmw.i16(i32 3, i16* %ptr, i16 %value, i32 6)
  ret i16 %1
}

; CHECK: @test_fetch_and_and_i16
define i16 @test_fetch_and_and_i16(i16* %ptr, i16 %value) {
  ; CHECK:   %res = and i16 %truncval, %value
  ; CHECK:   %res2 = and i16 %shval, %value
  %1 = call i16 @llvm.nacl.atomic.rmw.i16(i32 4, i16* %ptr, i16 %value, i32 6)
  ret i16 %1
}

; CHECK: @test_fetch_and_xor_i16
define i16 @test_fetch_and_xor_i16(i16* %ptr, i16 %value) {
  ; CHECK:   %res = xor i16 %truncval, %value
  ; CHECK:   %res2 = xor i16 %shval, %value
  %1 = call i16 @llvm.nacl.atomic.rmw.i16(i32 5, i16* %ptr, i16 %value, i32 6)
  ret i16 %1
}

; CHECK: @test_val_compare_and_swap_i16
define i16 @test_val_compare_and_swap_i16(i16* %ptr, i16 %oldval, i16 %newval) {
; CHECK-NEXT:  call void asm sideeffect "", "~{memory}"()
; CHECK-NEXT:  %uintptr = ptrtoint i16* %ptr to i32
; CHECK-NEXT:  %aligneduintptr = and i32 %uintptr, -4
; CHECK-NEXT:  %aligned32 = and i32 %uintptr, 3
; CHECK-NEXT:  %ptr32 = inttoptr i32 %aligneduintptr to i32*
; CHECK-NEXT:  %isaligned32 = icmp eq i32 %aligned32, 0
; CHECK-NEXT:  br i1 %isaligned32, label %atomic16aligned32, label %atomic16aligned16
;
; CHECK: atomic16successor:
; CHECK-NEXT:  %1 = phi i16 [ %truncval, %atomic16aligned32 ], [ %shval, %atomic16aligned16 ]
; CHECK-NEXT:  call void asm sideeffect "", "~{memory}"()
; CHECK-NEXT:  ret i16 %1
;
; CHECK: atomic16aligned32:
; CHECK-NEXT:  %loaded = load atomic i32* %ptr32 seq_cst, align 4
; CHECK-NEXT:  %truncval = trunc i32 %loaded to i16
; CHECK-NEXT:  %mergeres = zext i16 %newval to i32
; CHECK-NEXT:  %maskedloaded = and i32 %loaded, -65536
; CHECK-NEXT:  %finalres = or i32 %mergeres, %maskedloaded
; CHECK-NEXT:  %zext = zext i16 %oldval to i32
; CHECK-NEXT:  %expected = or i32 %maskedloaded, %zext
; CHECK-NEXT:  %cmpxchg.results = cmpxchg i32* %ptr32, i32 %expected, i32 %finalres seq_cst seq_cst
; CHECK-NEXT:  %success = extractvalue { i32, i1 } %cmpxchg.results, 1
; CHECK-NEXT:  br i1 %success, label %atomic16successor, label %atomic16aligned32
;
; CHECK: atomic16aligned16:
; CHECK-NEXT:  %loaded1 = load atomic i32* %ptr32 seq_cst, align 4
; CHECK-NEXT:  %lshr = lshr i32 %loaded1, 16
; CHECK-NEXT:  %shval = trunc i32 %lshr to i16
; CHECK-NEXT:  %zext2 = zext i16 %newval to i32
; CHECK-NEXT:  %mergeres3 = shl i32 %zext2, 16
; CHECK-NEXT:  %maskedloaded4 = and i32 %loaded1, 65535
; CHECK-NEXT:  %finalres5 = or i32 %mergeres3, %maskedloaded4
; CHECK-NEXT:  %zext6 = zext i16 %oldval to i32
; CHECK-NEXT:  %shl = shl i32 %zext6, 16
; CHECK-NEXT:  %expected7 = or i32 %maskedloaded4, %shl
; CHECK-NEXT:  %cmpxchg.results8 = cmpxchg i32* %ptr32, i32 %expected7, i32 %finalres5 seq_cst seq_cst
; CHECK-NEXT:  %success9 = extractvalue { i32, i1 } %cmpxchg.results8, 1
; CHECK-NEXT:  br i1 %success9, label %atomic16successor, label %atomic16aligned16
 %1 = call i16 @llvm.nacl.atomic.cmpxchg.i16(i16* %ptr, i16 %oldval, i16 %newval, i32 6, i32 6)
  ret i16 %1
}
