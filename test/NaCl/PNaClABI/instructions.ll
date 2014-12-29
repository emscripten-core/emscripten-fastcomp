; RUN: not pnacl-abicheck < %s | FileCheck %s
; Test instruction opcodes allowed by PNaCl ABI

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"

define internal void @terminators() {
; CHECK: ERROR: Function terminators
; Terminator instructions
terminators:
 ret void
 br i1 0, label %next2, label %next
next:
 switch i32 1, label %next2 [i32 0, label %next]
next2:
  unreachable
; CHECK-NOT: disallowed
; CHECK: Function terminators disallowed: bad instruction opcode: indirectbr
  indirectbr i8* undef, [label %next, label %next2]
}

define internal void @binops() {
; CHECK-NOT: ERROR: Function binops
; Binary operations
  %a1 = add i32 0, 0
  %a2 = fadd float 0.0, 0.0
  %a3 = sub i32 0, 0
  %a4 = fsub float 0.0, 0.0
  %a5 = mul i32 0, 0
  %a6 = fmul float 0.0, 0.0
  %a7 = udiv i32 0, 1
  %a8 = sdiv i32 0, 1
  %a9 = fdiv float 0.0, 1.0
  %a10 = urem i32 0, 1
  %a11 = srem i32 0, 1
  %a12 = frem float 0.0, 1.0
; Bitwise binary operations
  %a13 = shl i32 1, 1
  %a14 = lshr i32 1, 1
  %a15 = ashr i32 1, 1
  %a16 = and i32 1, 1
  %a17 = or i32 1, 1
  %a18 = xor i32 1, 1
  ret void
}
; CHECK-NOT: disallowed

define internal void @vector_binops(<4 x i32> %i, <4 x float> %f) {
; CHECK-NOT: ERROR: Function vector_binops
; Binary operations
  %a1 = add <4 x i32> %i, %i
  %a2 = fadd <4 x float> %f, %f
  %a3 = sub <4 x i32> %i, %i
  %a4 = fsub <4 x float> %f, %f
  %a5 = mul <4 x i32> %i, %i
  %a6 = fmul <4 x float> %f, %f
  %a7 = udiv <4 x i32> %i, %i
  %a8 = sdiv <4 x i32> %i, %i
  %a9 = fdiv <4 x float> %f, %f
  %a10 = urem <4 x i32> %i, %i
  %a11 = srem <4 x i32> %i, %i
  %a12 = frem <4 x float> %f, %f
; Bitwise binary operations
  %a13 = shl <4 x i32> %i, %i
  %a14 = lshr <4 x i32> %i, %i
  %a15 = ashr <4 x i32> %i, %i
  %a16 = and <4 x i32> %i, %i
  %a17 = or <4 x i32> %i, %i
  %a18 = xor <4 x i32> %i, %i
  ret void
}
; CHECK-NOT: disallowed

define internal void @vectors_ok(<4 x i32> %i) {
; CHECK-NOT: ERROR: Function vectors_ok
  %eu4xi32.0 = extractelement <4 x i32> undef, i32 0
  %eu4xi32.1 = extractelement <4 x i32> undef, i32 1
  %eu4xi32.2 = extractelement <4 x i32> undef, i32 2
  %eu4xi32.3 = extractelement <4 x i32> undef, i32 3

  %ev4xi32.0 = extractelement <4 x i32> %i, i32 0
  %ev4xi32.1 = extractelement <4 x i32> %i, i32 1
  %ev4xi32.2 = extractelement <4 x i32> %i, i32 2
  %ev4xi32.3 = extractelement <4 x i32> %i, i32 3

  %iu4xi32.0 = insertelement <4 x i32> undef, i32 1, i32 0
  %iu4xi32.1 = insertelement <4 x i32> undef, i32 1, i32 1
  %iu4xi32.2 = insertelement <4 x i32> undef, i32 1, i32 2
  %iu4xi32.3 = insertelement <4 x i32> undef, i32 1, i32 3

  %iv4xi32.0 = insertelement <4 x i32> %i, i32 1, i32 0
  %iv4xi32.1 = insertelement <4 x i32> %i, i32 1, i32 1
  %iv4xi32.2 = insertelement <4 x i32> %i, i32 1, i32 2
  %iv4xi32.3 = insertelement <4 x i32> %i, i32 1, i32 3

  ret void
}
; CHECK-NOT: disallowed

define internal void @vectors_bad(i32 %idx) {
; CHECK: ERROR: Function vectors_bad

  %e.var.idx = extractelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 %idx ; CHECK-NEXT: disallowed: non-constant vector insert/extract index: {{.*}} extractelement
  %e.oob.idx = extractelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 4 ; CHECK-NEXT: disallowed: out of range vector insert/extract index: {{.*}} extractelement
  %e.vec.imm = extractelement <4 x i32> <i32 0, i32 1, i32 2, i32 3>, i32 0 ; CHECK-NEXT: disallowed: bad operand: {{.*}} extractelement
  %i.var.idx = insertelement <4 x i32> undef, i32 42, i32 %idx ; CHECK-NEXT: disallowed: non-constant vector insert/extract index: {{.*}} insertelement
  %i.oob.idx = insertelement <4 x i32> undef, i32 42, i32 4 ; CHECK-NEXT: disallowed: out of range vector insert/extract index: {{.*}} insertelement
  %i.vec.imm = insertelement <4 x i32> <i32 0, i32 1, i32 2, i32 3>, i32 42, i32 0 ; CHECK-NEXT: disallowed: bad operand: {{.*}} insertelement
  %a3 = shufflevector <4 x i32> undef, <4 x i32> undef, <4 x i32> undef ; CHECK-NEXT: disallowed: bad instruction opcode: {{.*}} shufflevector

  ret void
}

define internal void @vectors_bad_zeroinitializer() {
; CHECK: ERROR: Function vectors_bad_zeroinitializer

  ; zeroinitializer isn't allowed, it should be globalized instead.
  %ez4xi32.0 = extractelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 0 ; CHECK-NEXT: disallowed: bad operand: {{.*}} extractelement {{.*}} zeroinitializer
  %ez4xi32.1 = extractelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 1 ; CHECK-NEXT: disallowed: bad operand: {{.*}} extractelement {{.*}} zeroinitializer
  %ez4xi32.2 = extractelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 2 ; CHECK-NEXT: disallowed: bad operand: {{.*}} extractelement {{.*}} zeroinitializer
  %ez4xi32.3 = extractelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 3 ; CHECK-NEXT: disallowed: bad operand: {{.*}} extractelement {{.*}} zeroinitializer
  %iz4xi32.0 = insertelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 1, i32 0 ; CHECK-NEXT: disallowed: bad operand: {{.*}} insertelement {{.*}} zeroinitializer
  %iz4xi32.1 = insertelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 1, i32 1 ; CHECK-NEXT: disallowed: bad operand: {{.*}} insertelement {{.*}} zeroinitializer
  %iz4xi32.2 = insertelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 1, i32 2 ; CHECK-NEXT: disallowed: bad operand: {{.*}} insertelement {{.*}} zeroinitializer
  %iz4xi32.3 = insertelement <4 x i32> <i32 0, i32 0, i32 0, i32 0>, i32 1, i32 3 ; CHECK-NEXT: disallowed: bad operand: {{.*}} insertelement {{.*}} zeroinitializer

  ret void
}

define internal void @aggregates() {
; CHECK: ERROR: Function aggregates

; Aggregate operations
  %a1 = extractvalue { i32, i32 } { i32 0, i32 0 }, 0
; CHECK: disallowed: bad instruction opcode: {{.*}} extractvalue

  %a2 = insertvalue {i32, float} undef, i32 1, 0
; CHECK-NEXT: disallowed: bad instruction opcode: {{.*}} insertvalue

  ret void
}

define internal void @memory() {
; CHECK: ERROR: Function memory
; Memory operations
  %a1 = alloca i8, i32 4
  %ptr = inttoptr i32 0 to i32*
  %a2 = load i32* %ptr, align 1
  store i32 undef, i32* %ptr, align 1
 ; CHECK-NOT: disallowed
  %a4 = getelementptr { i32, i32}* undef ; CHECK-NEXT: disallowed: bad instruction opcode: {{.*}} getelementptr
  ret void
}

define internal void @vector_memory() {
; CHECK: ERROR: Function vector_memory
; Vector memory operations.
  %ptr16xi8 = inttoptr i32 0 to <16 x i8>*
  %ptr8xi16 = inttoptr i32 0 to <8 x i16>*
  %ptr4xi32 = inttoptr i32 0 to <4 x i32>*
  %ptr4xfloat = inttoptr i32 0 to <4 x float>*

  %l16xi8 = load <16 x i8>* %ptr16xi8, align 1
  %l8xi16 = load <8 x i16>* %ptr8xi16, align 2
  %l4xi32 = load <4 x i32>* %ptr4xi32, align 4
  %l4xfloat = load <4 x float>* %ptr4xfloat, align 4

  store <16 x i8> undef, <16 x i8>* %ptr16xi8, align 1
  store <8 x i16> undef, <8 x i16>* %ptr8xi16, align 2
  store <4 x i32> undef, <4 x i32>* %ptr4xi32, align 4
  store <4 x float> undef, <4 x float>* %ptr4xfloat, align 4

 ; CHECK-NOT: disallowed
 ; The following vector types are disallowed.
  %ptr2xi1 = inttoptr i32 0 to <2 x i1>*         ; CHECK-NEXT: disallowed: bad result type: <2 x i1>*
  %ptr4xi1 = inttoptr i32 0 to <4 x i1>*         ; CHECK-NEXT: disallowed: bad result type: <4 x i1>*
  %ptr8xi1 = inttoptr i32 0 to <8 x i1>*	 ; CHECK-NEXT: disallowed: bad result type: <8 x i1>*
  %ptr16xi1 = inttoptr i32 0 to <16 x i1>*	 ; CHECK-NEXT: disallowed: bad result type: <16 x i1>*
  %ptr32xi1 = inttoptr i32 0 to <32 x i1>*       ; CHECK-NEXT: disallowed: bad result type: <32 x i1>*
  %ptr64xi1 = inttoptr i32 0 to <64 x i1>*       ; CHECK-NEXT: disallowed: bad result type: <64 x i1>*
  %ptr2xi8 = inttoptr i32 0 to <2 x i8>*         ; CHECK-NEXT: disallowed: bad result type: <2 x i8>*
  %ptr4xi8 = inttoptr i32 0 to <4 x i8>*         ; CHECK-NEXT: disallowed: bad result type: <4 x i8>*
  %ptr32xi8 = inttoptr i32 0 to <32 x i8>*       ; CHECK-NEXT: disallowed: bad result type: <32 x i8>*
  %ptr64xi8 = inttoptr i32 0 to <64 x i8>*       ; CHECK-NEXT: disallowed: bad result type: <64 x i8>*
  %ptr2xi16 = inttoptr i32 0 to <2 x i16>*       ; CHECK-NEXT: disallowed: bad result type: <2 x i16>*
  %ptr4xi16 = inttoptr i32 0 to <4 x i16>*       ; CHECK-NEXT: disallowed: bad result type: <4 x i16>*
  %ptr16xi16 = inttoptr i32 0 to <16 x i16>*     ; CHECK-NEXT: disallowed: bad result type: <16 x i16>*
  %ptr32xi16 = inttoptr i32 0 to <32 x i16>*     ; CHECK-NEXT: disallowed: bad result type: <32 x i16>*
  %ptr2xi32 = inttoptr i32 0 to <2 x i32>*       ; CHECK-NEXT: disallowed: bad result type: <2 x i32>*
  %ptr8xi32 = inttoptr i32 0 to <8 x i32>*       ; CHECK-NEXT: disallowed: bad result type: <8 x i32>*
  %ptr16xi32 = inttoptr i32 0 to <16 x i32>*     ; CHECK-NEXT: disallowed: bad result type: <16 x i32>*
  %ptr2xi64 = inttoptr i32 0 to <2 x i64>*       ; CHECK-NEXT: disallowed: bad result type: <2 x i64>*
  %ptr4xi64 = inttoptr i32 0 to <4 x i64>*       ; CHECK-NEXT: disallowed: bad result type: <4 x i64>*
  %ptr8xi64 = inttoptr i32 0 to <8 x i64>*       ; CHECK-NEXT: disallowed: bad result type: <8 x i64>*
  %ptr2xfloat = inttoptr i32 0 to <2 x float>*   ; CHECK-NEXT: disallowed: bad result type: <2 x float>*
  %ptr8xfloat = inttoptr i32 0 to <8 x float>*   ; CHECK-NEXT: disallowed: bad result type: <8 x float>*
  %ptr16xfloat = inttoptr i32 0 to <16 x float>* ; CHECK-NEXT: disallowed: bad result type: <16 x float>*
  %ptr2xdouble = inttoptr i32 0 to <2 x double>* ; CHECK-NEXT: disallowed: bad result type: <2 x double>*
  %ptr4xdouble = inttoptr i32 0 to <4 x double>* ; CHECK-NEXT: disallowed: bad result type: <4 x double>*
  %ptr8xdouble = inttoptr i32 0 to <8 x double>* ; CHECK-NEXT: disallowed: bad result type: <8 x double>*

  ; i1 vector pointers are simply disallowed, their alignment is inconsequential.
  %l4xi1 = load <4 x i1>* %ptr4xi1, align 1    ; CHECK-NEXT: disallowed: bad pointer: %l4xi1 = load <4 x i1>* %ptr4xi1, align 1
  %l8xi1 = load <8 x i1>* %ptr8xi1, align 1    ; CHECK-NEXT: disallowed: bad pointer: %l8xi1 = load <8 x i1>* %ptr8xi1, align 1
  %l16xi1 = load <16 x i1>* %ptr16xi1, align 1 ; CHECK-NEXT: disallowed: bad pointer: %l16xi1 = load <16 x i1>* %ptr16xi1, align 1

  store <4 x i1> undef, <4 x i1>* %ptr4xi1, align 1    ; CHECK-NEXT: disallowed: bad pointer: store <4 x i1> undef, <4 x i1>* %ptr4xi1, align 1
  store <8 x i1> undef, <8 x i1>* %ptr8xi1, align 1    ; CHECK-NEXT: disallowed: bad pointer: store <8 x i1> undef, <8 x i1>* %ptr8xi1, align 1
  store <16 x i1> undef, <16 x i1>* %ptr16xi1, align 1 ; CHECK-NEXT: disallowed: bad pointer: store <16 x i1> undef, <16 x i1>* %ptr16xi1, align 1

  ; Under- or over-aligned load/store are disallowed.
  %a1_8xi16 = load <8 x i16>* %ptr8xi16, align 1       ; CHECK-NEXT: disallowed: bad alignment: %a1_8xi16 = load <8 x i16>* %ptr8xi16, align 1
  %a1_4xi32 = load <4 x i32>* %ptr4xi32, align 1       ; CHECK-NEXT: disallowed: bad alignment:	%a1_4xi32 = load <4 x i32>* %ptr4xi32, align 1
  %a1_4xfloat = load <4 x float>* %ptr4xfloat, align 1 ; CHECK-NEXT: disallowed: bad alignment:	%a1_4xfloat = load <4 x float>* %ptr4xfloat, align 1

  %a16_16xi8 = load <16 x i8>* %ptr16xi8, align 16       ; CHECK-NEXT: disallowed: bad alignment: %a16_16xi8 = load <16 x i8>* %ptr16xi8, align 16
  %a16_8xi16 = load <8 x i16>* %ptr8xi16, align 16       ; CHECK-NEXT: disallowed: bad alignment: %a16_8xi16 = load <8 x i16>* %ptr8xi16, align 16
  %a16_4xi32 = load <4 x i32>* %ptr4xi32, align 16       ; CHECK-NEXT: disallowed: bad alignment: %a16_4xi32 = load <4 x i32>* %ptr4xi32, align 16
  %a16_4xfloat = load <4 x float>* %ptr4xfloat, align 16 ; CHECK-NEXT: disallowed: bad alignment: %a16_4xfloat = load <4 x float>* %ptr4xfloat, align 16

  store <8 x i16> undef, <8 x i16>* %ptr8xi16, align 1       ; CHECK-NEXT: disallowed: bad alignment: store <8 x i16> undef, <8 x i16>* %ptr8xi16, align 1
  store <4 x i32> undef, <4 x i32>* %ptr4xi32, align 1	     ; CHECK-NEXT: disallowed: bad alignment: store <4 x i32> undef, <4 x i32>* %ptr4xi32, align 1
  store <4 x float> undef, <4 x float>* %ptr4xfloat, align 1 ; CHECK-NEXT: disallowed: bad alignment: store <4 x float> undef, <4 x float>* %ptr4xfloat, align 1

  store <16 x i8> undef, <16 x i8>* %ptr16xi8, align 16       ; CHECK-NEXT: disallowed: bad alignment: store <16 x i8> undef, <16 x i8>* %ptr16xi8, align 16
  store <8 x i16> undef, <8 x i16>* %ptr8xi16, align 16	      ; CHECK-NEXT: disallowed: bad alignment: store <8 x i16> undef, <8 x i16>* %ptr8xi16, align 16
  store <4 x i32> undef, <4 x i32>* %ptr4xi32, align 16	      ; CHECK-NEXT: disallowed: bad alignment: store <4 x i32> undef, <4 x i32>* %ptr4xi32, align 16
  store <4 x float> undef, <4 x float>* %ptr4xfloat, align 16 ; CHECK-NEXT: disallowed: bad alignment: store <4 x float> undef, <4 x float>* %ptr4xfloat, align 16

 ret void
}

define internal void @atomic() {
; CHECK: ERROR: Function atomic
  %ptr = inttoptr i32 0 to i32*

 ; CHECK-NOT: disallowed
  %la = load atomic i32* %ptr seq_cst, align 4                      ; CHECK: disallowed: atomic load: {{.*}} load atomic
  %lv = load volatile i32* %ptr, align 4                            ; CHECK: disallowed: volatile load: {{.*}} load volatile
  store atomic i32 undef, i32* %ptr seq_cst, align 4                ; CHECK: disallowed: atomic store: store atomic
  store volatile i32 undef, i32* %ptr, align 4                      ; CHECK: disallowed: volatile store: store volatile
  fence acq_rel                                                     ; CHECK: disallowed: bad instruction opcode: fence
  %cmpx = cmpxchg i32* %ptr, i32 undef, i32 undef acq_rel monotonic ; CHECK: disallowed: bad instruction opcode: {{.*}} cmpxchg
  %crm = atomicrmw add i32* %ptr, i32 1 acquire                     ; CHECK: disallowed: bad instruction opcode: {{.*}} atomicrmw
  ret void
}

define internal void @atomic_vector() {
; CHECK: ERROR: Function atomic_vector
  %ptr = inttoptr i32 0 to <4 x i32>*

 ; CHECK-NOT: disallowed
  %la = load atomic <4 x i32>* %ptr seq_cst, align 1             ; CHECK: disallowed: atomic load: {{.*}} load atomic
  %lv = load volatile <4 x i32>* %ptr, align 1                   ; CHECK: disallowed: volatile load: {{.*}} load volatile
  store atomic <4 x i32> undef, <4 x i32>* %ptr seq_cst, align 1 ; CHECK: disallowed: atomic store: store atomic
  store volatile <4 x i32> undef, <4 x i32>* %ptr, align 1       ; CHECK: disallowed: volatile store: store volatile
  ret void
}

define internal void @conversion() {
; CHECK-NOT: Function conversion
; Conversion operations
  %t = trunc i32 undef to i8
  %z = zext i8 undef to i32
  %s = sext i8 undef to i32
  %ft = fptrunc double undef to float
  %fe = fpext float undef to double
  %fu32 = fptoui float undef to i32
  %fs32 = fptosi float undef to i32
  %fu64 = fptoui double undef to i64
  %fs64 = fptosi double undef to i64
  %uf32 = uitofp i32 undef to float
  %sf32 = sitofp i32 undef to float
  %uf64 = uitofp i64 undef to double
  %sf64 = sitofp i64 undef to double
  ret void
}
; CHECK-NOT: disallowed

define internal void @vector_conversion() {
; CHECK: ERROR: Function vector_conversion
  %t1 = trunc <4 x i32> undef to <4 x i16> ; CHECK-NEXT: bad result type: <4 x i16>
  %t2 = trunc <4 x i32> undef to <4 x i8> ; CHECK-NEXT: bad result type: <4 x i8>
  %t3 = trunc <8 x i16> undef to <8 x i8> ; CHECK-NEXT: bad result type: <8 x i8>
  %z1 = zext <8 x i16> undef to <8 x i32> ; CHECK-NEXT: bad result type: <8 x i32>
  %z2 = zext <8 x i1> undef to <8 x i32> ; CHECK-NEXT: bad result type: <8 x i32>
  %z4 = zext <8 x i1> undef to <8 x i8> ; CHECK-NEXT: bad result type: <8 x i8>
  %z5 = zext <16 x i8> undef to <16 x i16> ; CHECK-NEXT: bad result type: <16 x i16>
  %z6 = zext <16 x i8> undef to <16 x i32> ; CHECK-NEXT: bad result type: <16 x i32>
  %z7 = zext <16 x i1> undef to <16 x i32> ; CHECK-NEXT: bad result type: <16 x i32>
  %z8 = zext <16 x i1> undef to <16 x i16> ; CHECK-NEXT: bad result type: <16 x i16>
  %s1 = sext <8 x i16> undef to <8 x i32> ; CHECK-NEXT: bad result type: <8 x i32>
  %s2 = sext <8 x i1> undef to <8 x i32> ; CHECK-NEXT: bad result type: <8 x i32>
  %s4 = sext <8 x i1> undef to <8 x i8> ; CHECK-NEXT: bad result type: <8 x i8>
  %s5 = sext <16 x i8> undef to <16 x i16> ; CHECK-NEXT: bad result type: <16 x i16>
  %s6 = sext <16 x i8> undef to <16 x i32> ; CHECK-NEXT: bad result type: <16 x i32>
  %s7 = sext <16 x i1> undef to <16 x i32> ; CHECK-NEXT: bad result type: <16 x i32>
  %s8 = sext <16 x i1> undef to <16 x i16> ; CHECK-NEXT: bad result type: <16 x i16>
  %ft = fptrunc <2 x double> undef to <2 x float> ; CHECK-NEXT: bad operand
  %fe = fpext <4 x float> undef to <4 x double> ; CHECK-NEXT: bad result type: <4 x double>
  %fu64 = fptoui <2 x double> undef to <2 x i64> ; CHECK-NEXT: bad operand
  %fs64 = fptosi <2 x double> undef to <2 x i64> ; CHECK-NEXT: bad operand
  %uf64 = uitofp <2 x i64> undef to <2 x double> ; CHECK-NEXT: bad operand
  %sf64 = sitofp <2 x i64> undef to <2 x double> ; CHECK-NEXT: bad operand

  ; Conversions to allowed types should work.
  %t4 = trunc <4 x i32> undef to <4 x i1> ; CHECK-NOT: bad
  %t5 = trunc <8 x i16> undef to <8 x i1> ; CHECK-NOT: bad
  %t6 = trunc <16 x i8> undef to <16 x i1> ; CHECK-NOT: bad
  %z10 = zext <4 x i1> undef to <4 x i32> ; CHECK-NOT: bad
  %z11 = zext <8 x i1> undef to <8 x i16> ; CHECK-NOT: bad
  %z12 = zext <16 x i1> undef to <16 x i8> ; CHECK-NOT: bad
  %s10 = sext <4 x i1> undef to <4 x i32> ; CHECK-NOT: bad
  %s11 = sext <8 x i1> undef to <8 x i16> ; CHECK-NOT: bad
  %s12 = sext <16 x i1> undef to <16 x i8> ; CHECK-NOT: bad
  %fu32 = fptoui <4 x float> undef to <4 x i32> ; CHECK-NOT: bad
  %fs32 = fptosi <4 x float> undef to <4 x i32> ; CHECK-NOT: bad
  %uf32 = uitofp <4 x i32> undef to <4 x float> ; CHECK-NOT: bad
  %sf32 = sitofp <4 x i32> undef to <4 x float> ; CHECK-NOT: bad
  ret void
}
; CHECK-NOT: disallowed

define internal void @other() {
; CHECK-NOT: Function other
entry:
  %icmp8 = icmp eq i8 undef, undef
  %icmp16 = icmp eq i16 undef, undef
  %icmp32 = icmp eq i32 undef, undef
  %icmp64 = icmp eq i64 undef, undef
  %vicmp8 = icmp eq <16 x i8> undef, undef
  %vicmp16 = icmp eq <8 x i16> undef, undef
  %vicmp32 = icmp eq <4 x i32> undef, undef
  %fcmp32 = fcmp oeq float undef, undef
  %fcmp64 = fcmp oeq double undef, undef
  %vfcmp = fcmp oeq <4 x float> undef, undef
  br i1 undef, label %foo, label %bar
foo:
; phi predecessor labels have to match to appease module verifier
  %phi1 = phi i1 [0, %entry], [0, %foo]
  %phi8 = phi i8 [0, %entry], [0, %foo]
  %phi16 = phi i16 [0, %entry], [0, %foo]
  %phi32 = phi i32 [0, %entry], [0, %foo]
  %phi64 = phi i64 [0, %entry], [0, %foo]
  %vphi4x1 = phi <4 x i1> [undef, %entry], [undef, %foo]
  %vphi8x1 = phi <8 x i1> [undef, %entry], [undef, %foo]
  %vphi16x1 = phi <16 x i1> [undef, %entry], [undef, %foo]
  %vphi8 = phi <16 x i8> [undef, %entry], [undef, %foo]
  %vphi16 = phi <8 x i16> [undef, %entry], [undef, %foo]
  %vphi32 = phi <4 x i32> [undef, %entry], [undef, %foo]

  %select = select i1 true, i8 undef, i8 undef

  %vselect4x1 = select i1 true, <4 x i1> undef, <4 x i1> undef
  %vselect8x1 = select i1 true, <8 x i1> undef, <8 x i1> undef
  %vselect16x1 = select i1 true, <16 x i1> undef, <16 x i1> undef
  %vselect8 = select i1 true, <16 x i8> undef, <16 x i8> undef
  %vselect16 = select i1 true, <8 x i16> undef, <8 x i16> undef
  %vselect32 = select i1 true, <4 x i32> undef, <4 x i32> undef

  %vvselect4x1 = select <4 x i1> undef, <4 x i1> undef, <4 x i1> undef
  %vvselect8x1 = select <8 x i1> undef, <8 x i1> undef, <8 x i1> undef
  %vvselect16x1 = select <16 x i1> undef, <16 x i1> undef, <16 x i1> undef
  %vvselect8 = select <16 x i1> undef, <16 x i8> undef, <16 x i8> undef
  %vvselect16 = select <8 x i1> undef, <8 x i16> undef, <8 x i16> undef
  %vvselect32 = select <4 x i1> undef, <4 x i32> undef, <4 x i32> undef

  call void @conversion()
  br i1 undef, label %foo, label %bar
bar:
  ret void
}
; CHECK-NOT: disallowed

define internal void @throwing_func() {
; CHECK-NOT: Function throwing_func
  ret void
}
; CHECK-NOT: disallowed

define internal void @personality_func() {
; CHECK-NOT: Function personality_func
  ret void
}
; CHECK-NOT: disallowed

define internal void @invoke_func() {
; CHECK: ERROR: Function invoke_func
  invoke void @throwing_func() to label %ok unwind label %onerror
; CHECK-NOT: disallowed
; CHECK: disallowed: bad instruction opcode: invoke
ok:
  ret void
onerror:
  %lp = landingpad i32
      personality i8* bitcast (void ()* @personality_func to i8*)
      catch i32* null
; CHECK: disallowed: bad instruction opcode: {{.*}} landingpad
  resume i32 %lp
; CHECK: disallowed: bad instruction opcode: resume
}

define internal i32 @va_arg(i32 %va_list_as_int) {
; CHECK: ERROR: Function va_arg
  %va_list = inttoptr i32 %va_list_as_int to i8*
  %val = va_arg i8* %va_list, i32
  ret i32 %val
}
; CHECK-NOT: disallowed
; CHECK: disallowed: bad instruction opcode: {{.*}} va_arg

@global_var = internal global [4 x i8] zeroinitializer

define internal void @constantexpr() {
; CHECK: ERROR: Function constantexpr
  ptrtoint i8* getelementptr ([4 x i8]* @global_var, i32 1, i32 0) to i32
  ret void
}
; CHECK-NOT: disallowed
; CHECK: disallowed: operand not InherentPtr: %1 = ptrtoint i8* getelementptr

define internal void @inline_asm() {
; CHECK: ERROR: Function inline_asm
  call void asm "foo", ""()
  ret void
}
; CHECK-NOT: disallowed
; CHECK: disallowed: inline assembly: call void asm "foo", ""()

; CHECK-NOT: disallowed
; If another check is added, there should be a check-not in between each check
