; RUN: pnacl-abicheck < %s | FileCheck %s
; Test types allowed by PNaCl ABI


; CHECK: Function badReturn has disallowed type: half* ()
declare half* @badReturn()

; CHECK: Function badArgType1 has disallowed type: void (half, i32)
declare void @badArgType1(half %a, i32 %b)
; CHECK: Function badArgType2 has disallowed type: void (i32, half)
declare void @badArgType2(i32 %a, half %b)


define void @func() {
entry:
  br label %block
block:

  ; We test for allowed/disallowed types via phi nodes.  This gives us
  ; a uniform way to test any type.

  ; Allowed types

  phi i1 [ undef, %entry ]
  phi i8 [ undef, %entry ]
  phi i16 [ undef, %entry ]
  phi i32 [ undef, %entry ]
  phi i64 [ undef, %entry ]
  phi float [ undef, %entry ]
  phi double [ undef, %entry ]
; CHECK-NOT: disallowed


  ; Disallowed integer types

  phi i4 [ undef, %entry ]
; CHECK: Function func has instruction with disallowed type: i4

  phi i33 [ undef, %entry ]
; CHECK: instruction with disallowed type: i33

  phi i128 [ undef, %entry ]
; CHECK: instruction with disallowed type: i128


  ; Disallowed floating point types

  phi half [ undef, %entry ]
; CHECK: instruction with disallowed type: half

  phi x86_fp80 [ undef, %entry ]
; CHECK: instruction with disallowed type: x86_fp80

  phi fp128 [ undef, %entry ]
; CHECK: instruction with disallowed type: fp128

  phi ppc_fp128 [ undef, %entry ]
; CHECK: instruction with disallowed type: ppc_fp128

  phi x86_mmx [ undef, %entry ]
; CHECK: instruction with disallowed type: x86_mmx
; CHECK: instruction operand with disallowed type: x86_mmx


  ; Derived types

  ; TODO(mseaborn): These are currently allowed but should be disallowed.
  phi i32* [ undef, %entry ]
  phi [1 x i32] [ undef, %entry ]
  phi { i32, float } [ undef, %entry ]
  phi void (i32)* [ undef, %entry ]
  phi <{ i8, i32 }> [ undef, %entry ]
  phi { i32, { i32, double }, float } [ undef, %entry ]
; CHECK-NOT: disallowed

  ; Derived types containing disallowed types
  phi half* [ undef, %entry ]
; CHECK: instruction with disallowed type: half*
  phi [2 x i33] [ undef, %entry ]
; CHECK: instruction with disallowed type: [2 x i33]
  phi { half, i32 } [ undef, %entry ]
; CHECK: instruction with disallowed type: { half, i32 }
  phi { float, i33 } [ undef, %entry ]
; CHECK: instruction with disallowed type: { float, i33 }
  phi { i32, { i32, half }, float } [ undef, %entry ]
; CHECK: instruction with disallowed type: { i32, { i32, half }, float }

  ; Vector types are disallowed
  phi <2 x i32> [ undef, %entry ]
; CHECK: instruction with disallowed type: <2 x i32>

  ret void
}


; named types. with the current implementation, bogus named types are legal
; until they are actually attempted to be used. Might want to fix that.
%struct.s1 = type { half, float}
%struct.s2 = type { i32, i32}

define void @func2() {
entry:
  br label %block
block:

  phi %struct.s1 [ undef, %entry ]
; CHECK: instruction with disallowed type: %struct.s1 = type { half, float }
; CHECK: instruction operand with disallowed type: %struct.s1 = type { half, float }

  phi %struct.s2 [ undef, %entry ]
; CHECK-NOT: disallowed

  ret void
}


; Circularities:  here to make sure the verifier doesn't crash or assert.

; This oddity is perfectly legal according to the IR and ABI verifiers.
; Might want to fix that. (good luck initializing one of these, though.)
%struct.snake = type { i32, %struct.tail }
%struct.tail = type { %struct.snake, i32 }

%struct.linked = type { i32, %struct.linked * }

define void @func3() {
entry:
  br label %block
block:

  phi %struct.snake [ undef, %entry ]
  phi %struct.linked [ undef, %entry ]
; CHECK-NOT: disallowed

  ret void
}
