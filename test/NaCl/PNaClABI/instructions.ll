; RUN: pnacl-abicheck < %s | FileCheck %s
; Test instruction opcodes allowed by PNaCl ABI

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"

define internal void @terminators() {
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
; Binary operations
  %a1 = add i32 0, 0
  %a2 = sub i32 0, 0
  %a3 = fsub float 0.0, 0.0
  %a4 = mul i32 0, 0
  %a5 = fmul float 0.0, 0.0
  %a6 = udiv i32 0, 1
  %a7 = sdiv i32 0, 1
  %a8 = fdiv float 0.0, 1.0
  %a9 = urem i32 0, 1
  %a10 = srem i32 0, 1
  %a11 = frem float 0.0, 1.0
; Bitwise binary operations
  %a12 = shl i32 1, 1
  %a13 = lshr i32 1, 1
  %a14 = ashr i32 1, 1
  %a15 = and i32 1, 1
  %a16 = or i32 1, 1
  %a17 = xor i32 1, 1
  ret void
}

define internal void @vectors() {
; CHECK-NOT: disallowed

; CHECK: disallowed: bad instruction opcode: {{.*}} extractelement
  %a1 = extractelement <2 x i32> <i32 0, i32 0>, i32 0

; CHECK: disallowed: bad instruction opcode: {{.*}} shufflevector
  %a2 = shufflevector <2 x i32> undef, <2 x i32> undef, <2 x i32> undef

; CHECK: disallowed: bad instruction opcode: {{.*}} insertelement
  %a3 = insertelement <2 x i32> undef, i32 1, i32 0

  ret void
}

define internal void @aggregates() {
; CHECK-NOT: disallowed

; Aggregate operations
  %a1 = extractvalue { i32, i32 } { i32 0, i32 0 }, 0
; CHECK: disallowed: bad instruction opcode: {{.*}} extractvalue

  %a2 = insertvalue {i32, float} undef, i32 1, 0
; CHECK-NEXT: disallowed: bad instruction opcode: {{.*}} insertvalue

  ret void
}

define internal void @memory() {
; Memory operations
  %a1 = alloca i8, i32 4
  %ptr = inttoptr i32 0 to i32*
  %a2 = load i32* %ptr, align 1
  store i32 undef, i32* %ptr, align 1
; CHECK-NOT: disallowed
; CHECK: disallowed: bad instruction opcode: {{.*}} getelementptr
  %a3 = getelementptr { i32, i32}* undef
  ret void
}

define internal void @atomic() {
  %a1 = alloca i8, i32 4
  %ptr = inttoptr i32 0 to i32*
 ; CHECK: disallowed: atomic load: {{.*}} load atomic
  %a2 = load atomic i32* %ptr seq_cst, align 4
; CHECK: disallowed: volatile load: {{.*}} load volatile
  %a3 = load volatile i32* %ptr, align 4
; CHECK: disallowed: atomic store: store atomic
  store atomic i32 undef, i32* %ptr seq_cst, align 4
; CHECK: disallowed: volatile store: store volatile
  store volatile i32 undef, i32* %ptr, align 4
; CHECK: disallowed: bad instruction opcode: fence
  fence acq_rel
; CHECK: disallowed: bad instruction opcode: {{.*}} cmpxchg
  %a4 = cmpxchg i32* %ptr, i32 undef, i32 undef acq_rel
; CHECK: disallowed: bad instruction opcode: {{.*}} atomicrmw
  %a5 = atomicrmw add i32* %ptr, i32 1 acquire
  ret void
}

define internal void @conversion() {
; Conversion operations
  %a1 = trunc i32 undef to i8
  %a2 = zext i8 undef to i32
  %a3 = sext i8 undef to i32
  %a4 = fptrunc double undef to float
  %a5 = fpext float undef to double
  %a6 = fptoui double undef to i64
  %a7 = fptosi double undef to i64
  %a8 = uitofp i64 undef to double
  %a9 = sitofp i64 undef to double
  ret void
}

define internal void @other() {
entry:
  %a1 = icmp eq i32 undef, undef
  %a2 = fcmp oeq float undef, undef
  br i1 undef, label %foo, label %bar
foo:
; phi predecessor labels have to match to appease module verifier
  %a3 = phi i32 [0, %entry], [0, %foo]
  %a4 = select i1 true, i8 undef, i8 undef
  call void @conversion()
  br i1 undef, label %foo, label %bar
bar:
  ret void
}

define internal void @throwing_func() {
  ret void
}
define internal void @personality_func() {
  ret void
}

define internal void @invoke_func() {
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
  %va_list = inttoptr i32 %va_list_as_int to i8*
  %val = va_arg i8* %va_list, i32
  ret i32 %val
}
; CHECK-NOT: disallowed
; CHECK: disallowed: bad instruction opcode: {{.*}} va_arg

@global_var = internal global [4 x i8] zeroinitializer

define internal void @constantexpr() {
  ptrtoint i8* getelementptr ([4 x i8]* @global_var, i32 1, i32 0) to i32
  ret void
}
; CHECK-NOT: disallowed
; CHECK: disallowed: operand not InherentPtr: %1 = ptrtoint i8* getelementptr

define internal void @inline_asm() {
  call void asm "foo", ""()
  ret void
}
; CHECK-NOT: disallowed
; CHECK: disallowed: inline assembly: call void asm "foo", ""()

; CHECK-NOT: disallowed
; If another check is added, there should be a check-not in between each check
