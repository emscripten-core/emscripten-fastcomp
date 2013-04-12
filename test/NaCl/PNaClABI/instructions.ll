; RUN: pnacl-abicheck < %s | FileCheck %s
; Test instruction opcodes allowed by PNaCl ABI
; No testing yet of operands, types, attributes, etc

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"
target triple = "le32-unknown-nacl"

define void @terminators() {
; Terminator instructions
terminators:
 ret void
 br i1 0, label %next2, label %next
next:
 switch i32 1, label %next2 [i32 0, label %next]
next2:
  unreachable
  resume i8 0
; CHECK-NOT: disallowed
; CHECK: Function terminators has disallowed instruction: indirectbr
  indirectbr i8* undef, [label %next, label %next2]
}

define void @binops() {
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

define void @vectors() {
; CHECK-NOT: disallowed
; CHECK: Function vectors has disallowed instruction: extractelement
  %a1 = extractelement <2 x i32> <i32 0, i32 0>, i32 0
; CHECK: Function vectors has disallowed instruction: shufflevector
  %a2 = shufflevector <2 x i32> undef, <2 x i32> undef, <2 x i32> undef
; CHECK: Function vectors has disallowed instruction: insertelement
; CHECK: Function vectors has instruction with disallowed type
; CHECK: Function vectors has instruction operand with disallowed type
  %a3 = insertelement <2 x i32> undef, i32 1, i32 0
  ret void
}

define void @aggregates() {
; Aggregate operations
  %a1 = extractvalue { i32, i32 } { i32 0, i32 0 }, 0
  %a2 = insertvalue {i32, float} undef, i32 1, 0
  ret void
}

define void @memory() {
; Memory operations
  %a1 = alloca i32
  %a2 = load i32* undef
  store i32 undef, i32* undef
  fence acq_rel
  %a3 = cmpxchg i32* undef, i32 undef, i32 undef acq_rel
  %a4 = atomicrmw add i32* undef, i32 1 acquire
; CHECK-NOT: disallowed
; CHECK: Function memory has disallowed instruction: getelementptr
  %a5 = getelementptr { i32, i32}* undef
  ret void
}

define void @conversion() {
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
  %a10 = ptrtoint i8* undef to i32
  %a11 = inttoptr i32 undef to i8*
  %a12 = bitcast i8* undef to i32*
  ret void
}

define void @other() {
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

declare void @external_func()
declare void @personality_func()

define void @invoke_func() {
  invoke void @external_func() to label %ok unwind label %onerror
; CHECK-NOT: disallowed
; CHECK: Function invoke_func has disallowed instruction: invoke
ok:
  ret void
onerror:
  %lp = landingpad i32
      personality i8* bitcast (void ()* @personality_func to i8*)
      catch i32* null
; CHECK-NEXT: Function invoke_func has disallowed instruction: landingpad
  ret void
}

define i32 @va_arg(i8* %va_list) {
  %val = va_arg i8* %va_list, i32
  ret i32 %val
}
; CHECK-NOT: disallowed
; CHECK: Function va_arg has disallowed instruction: va_arg

; CHECK-NOT: disallowed
; If another check is added, there should be a check-not in between each check
