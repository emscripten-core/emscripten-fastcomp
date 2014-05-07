; Tests that vector operations survive through PNaCl bitcode files.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - \
; RUN:              | FileCheck %s

define internal void @binops() {      ; CHECK-LABEL: binops
  %1 = add <4 x i32> undef, undef     ; CHECK-NEXT: %1 = add <4 x i32> undef, undef
  %2 = fadd <4 x float> undef, undef  ; CHECK-NEXT: %2 = fadd <4 x float> undef, undef
  %3 = sub <4 x i32> undef, undef     ; CHECK-NEXT: %3 = sub <4 x i32> undef, undef
  %4 = fsub <4 x float> undef, undef  ; CHECK-NEXT: %4 = fsub <4 x float> undef, undef
  %5 = mul <4 x i32> undef, undef     ; CHECK-NEXT: %5 = mul <4 x i32> undef, undef
  %6 = fmul <4 x float> undef, undef  ; CHECK-NEXT: %6 = fmul <4 x float> undef, undef
  %7 = udiv <4 x i32> undef, undef    ; CHECK-NEXT: %7 = udiv <4 x i32> undef, undef
  %8 = sdiv <4 x i32> undef, undef    ; CHECK-NEXT: %8 = sdiv <4 x i32> undef, undef
  %9 = fdiv <4 x float> undef, undef  ; CHECK-NEXT: %9 = fdiv <4 x float> undef, undef
  %10 = urem <4 x i32> undef, undef   ; CHECK-NEXT: %10 = urem <4 x i32> undef, undef
  %11 = srem <4 x i32> undef, undef   ; CHECK-NEXT: %11 = srem <4 x i32> undef, undef
  %12 = frem <4 x float> undef, undef ; CHECK-NEXT: %12 = frem <4 x float> undef, undef
  %13 = shl <4 x i32> undef, undef    ; CHECK-NEXT: %13 = shl <4 x i32> undef, undef
  %14 = lshr <4 x i32> undef, undef   ; CHECK-NEXT: %14 = lshr <4 x i32> undef, undef
  %15 = ashr <4 x i32> undef, undef   ; CHECK-NEXT: %15 = ashr <4 x i32> undef, undef
  %16 = and <4 x i32> undef, undef    ; CHECK-NEXT: %16 = and <4 x i32> undef, undef
  %17 = or <4 x i32> undef, undef     ; CHECK-NEXT: %17 = or <4 x i32> undef, undef
  %18 = xor <4 x i32> undef, undef    ; CHECK-NEXT: %18 = xor <4 x i32> undef, undef
  %19 = select <4 x i1> undef, <4 x i32> undef, <4 x i32> undef ; CHECK-NEXT: %19 = select <4 x i1> undef, <4 x i32> undef, <4 x i32> undef
  ret void                            ; CHECK-NEXT: ret void
}

define internal void @insert_extract() { ; CHECK-LABEL: insert_extract
  %1 = extractelement <4 x i32> undef, i32 0       ; CHECK-NEXT: %1 = extractelement <4 x i32> undef, i32 0
  %2 = extractelement <4 x i32> undef, i32 1       ; CHECK-NEXT: %2 = extractelement <4 x i32> undef, i32 1
  %3 = extractelement <4 x i32> undef, i32 2       ; CHECK-NEXT: %3 = extractelement <4 x i32> undef, i32 2
  %4 = extractelement <4 x i32> undef, i32 3       ; CHECK-NEXT: %4 = extractelement <4 x i32> undef, i32 3
  %5 = insertelement <4 x i32> undef, i32 1, i32 0 ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 1, i32 0
  %6 = insertelement <4 x i32> undef, i32 1, i32 1 ; CHECK-NEXT: %6 = insertelement <4 x i32> undef, i32 1, i32 1
  %7 = insertelement <4 x i32> undef, i32 1, i32 2 ; CHECK-NEXT: %7 = insertelement <4 x i32> undef, i32 1, i32 2
  %8 = insertelement <4 x i32> undef, i32 1, i32 3 ; CHECK-NEXT: %8 = insertelement <4 x i32> undef, i32 1, i32 3
  ret void                                         ; CHECK-NEXT: ret void
}
