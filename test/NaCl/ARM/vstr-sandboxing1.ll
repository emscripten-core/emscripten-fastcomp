; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-store -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define void @test_vstr_sandbox(<8 x i8>* %ptr) nounwind {
  %1 = insertelement <8 x i8> undef, i8 -128, i32 0
  %2 = shufflevector <8 x i8> %1, <8 x i8> undef, <8 x i32> zeroinitializer
  store <8 x i8> %2, <8 x i8>* %ptr, align 8
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vstr {{[0-9a-z]+}}, [r0]

  ret void
}

