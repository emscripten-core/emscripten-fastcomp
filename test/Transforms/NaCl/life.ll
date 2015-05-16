; RUN: opt -pnacl-abi-simplify-postopt %s -S | \
; RUN:   opt -backend-canonicalize -S | FileCheck %s

; Test that the SIMD game of life example from the NaCl SDK has an inner loop
; that contains the expected shufflevector instructions. First run the ABI
; simplifications on the code, then run the translator's peepholes.
;
; The stable PNaCl bitcode ABI doesn't have shufflevector nor constant vectors,
; it instead has insertelement, extractelement and load from globals. Note that
; `undef` becomes `0` in the constants.

; The datalayout is needed to determine the alignment of the globals.
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

define <16 x i8> @InnerLoop(<16 x i8>* %pixel_line, <16 x i8> %src00, <16 x i8> %src01, <16 x i8> %src10, <16 x i8> %src11, <16 x i8> %src20, <16 x i8> %src21) {
  ; CHECK-LABEL: InnerLoop
  ; CHECK-NEXT: shufflevector <16 x i8> %src00, <16 x i8> %src01, <16 x i32> <i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16>
  ; CHECK-NEXT: shufflevector <16 x i8> %src00, <16 x i8> %src01, <16 x i32> <i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17>
  ; CHECK-NEXT: shufflevector <16 x i8> %src10, <16 x i8> %src11, <16 x i32> <i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16>
  ; CHECK-NEXT: shufflevector <16 x i8> %src10, <16 x i8> %src11, <16 x i32> <i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17>
  ; CHECK-NEXT: shufflevector <16 x i8> %src20, <16 x i8> %src21, <16 x i32> <i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16>
  ; CHECK-NEXT: shufflevector <16 x i8> %src20, <16 x i8> %src21, <16 x i32> <i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17>
  ; CHECK-NOT: load
  ; CHECK-NOT: insertelement
  ; CHECK-NOT: extractelement
  %shuffle = shufflevector <16 x i8> %src00, <16 x i8> %src01, <16 x i32> <i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16>
  %shuffle3 = shufflevector <16 x i8> %src00, <16 x i8> %src01, <16 x i32> <i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17>
  %shuffle4 = shufflevector <16 x i8> %src10, <16 x i8> %src11, <16 x i32> <i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16>
  %shuffle5 = shufflevector <16 x i8> %src10, <16 x i8> %src11, <16 x i32> <i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17>
  %shuffle6 = shufflevector <16 x i8> %src20, <16 x i8> %src21, <16 x i32> <i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16>
  %shuffle7 = shufflevector <16 x i8> %src20, <16 x i8> %src21, <16 x i32> <i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17>
  %add = add <16 x i8> %shuffle, %src00
  %add8 = add <16 x i8> %add, %shuffle3
  %add9 = add <16 x i8> %add8, %src10
  %add10 = add <16 x i8> %add9, %shuffle5
  %add11 = add <16 x i8> %add10, %src20
  %add12 = add <16 x i8> %add11, %shuffle6
  %add13 = add <16 x i8> %add12, %shuffle7
  %add14 = shl <16 x i8> %add13, <i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1>
  %add15 = add <16 x i8> %add14, %shuffle4
  %cmp = icmp ugt <16 x i8> %add15, <i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4, i8 4>
  %sext = sext <16 x i1> %cmp to <16 x i8>
  %cmp16 = icmp ult <16 x i8> %add15, <i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8, i8 8>
  ; CHECK: select
  %and = select <16 x i1> %cmp16, <16 x i8> %sext, <16 x i8> zeroinitializer
  ; CHECK-NEXT: shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0>, <16 x i32> <i32 16, i32 0, i32 16, i32 17, i32 16, i32 1, i32 16, i32 17, i32 16, i32 2, i32 16, i32 17, i32 16, i32 3, i32 16, i32 17>
  ; CHECK-NEXT: shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0>, <16 x i32> <i32 16, i32 4, i32 16, i32 17, i32 16, i32 5, i32 16, i32 17, i32 16, i32 6, i32 16, i32 17, i32 16, i32 7, i32 16, i32 17>
  ; CHECK-NEXT: shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0>, <16 x i32> <i32 16, i32 8, i32 16, i32 17, i32 16, i32 9, i32 16, i32 17, i32 16, i32 10, i32 16, i32 17, i32 16, i32 11, i32 16, i32 17>
  ; CHECK-NEXT: shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0>, <16 x i32> <i32 16, i32 12, i32 16, i32 17, i32 16, i32 13, i32 16, i32 17, i32 16, i32 14, i32 16, i32 17, i32 16, i32 15, i32 16, i32 17>
  ; CHECK-NOT: load
  ; CHECK-NOT: insertelement
  ; CHECK-NOT: extractelement
  %shuffle18 = shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef>, <16 x i32> <i32 16, i32 0, i32 16, i32 17, i32 16, i32 1, i32 16, i32 17, i32 16, i32 2, i32 16, i32 17, i32 16, i32 3, i32 16, i32 17>
  %shuffle19 = shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef>, <16 x i32> <i32 16, i32 4, i32 16, i32 17, i32 16, i32 5, i32 16, i32 17, i32 16, i32 6, i32 16, i32 17, i32 16, i32 7, i32 16, i32 17>
  %shuffle20 = shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef>, <16 x i32> <i32 16, i32 8, i32 16, i32 17, i32 16, i32 9, i32 16, i32 17, i32 16, i32 10, i32 16, i32 17, i32 16, i32 11, i32 16, i32 17>
  %shuffle21 = shufflevector <16 x i8> %and, <16 x i8> <i8 0, i8 -1, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef, i8 undef>, <16 x i32> <i32 16, i32 12, i32 16, i32 17, i32 16, i32 13, i32 16, i32 17, i32 16, i32 14, i32 16, i32 17, i32 16, i32 15, i32 16, i32 17>
  store <16 x i8> %shuffle18, <16 x i8>* %pixel_line, align 16
  %add.ptr22 = getelementptr inbounds <16 x i8>, <16 x i8>* %pixel_line, i32 1
  store <16 x i8> %shuffle19, <16 x i8>* %add.ptr22, align 16
  %add.ptr23 = getelementptr inbounds <16 x i8>, <16 x i8>* %pixel_line, i32 2
  store <16 x i8> %shuffle20, <16 x i8>* %add.ptr23, align 16
  %add.ptr24 = getelementptr inbounds <16 x i8>, <16 x i8>* %pixel_line, i32 3
  store <16 x i8> %shuffle21, <16 x i8>* %add.ptr24, align 16
  %and25 = and <16 x i8> %and, <i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1>
  ret <16 x i8> %and25
}
