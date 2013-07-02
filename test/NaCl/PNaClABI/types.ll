; RUN: pnacl-abicheck < %s | FileCheck %s
; Test types allowed by PNaCl ABI


; CHECK: Function badReturn has disallowed type: half* ()
define internal half* @badReturn() {
  unreachable
}

; CHECK: Function badArgType1 has disallowed type: void (half, i32)
define internal void @badArgType1(half %a, i32 %b) {
  ret void
}
; CHECK: Function badArgType2 has disallowed type: void (i32, half)
define internal void @badArgType2(i32 %a, half %b) {
  ret void
}


define internal void @func() {
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
; CHECK: Function func disallowed: bad operand: {{.*}} i4

  phi i33 [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} i33

  phi i128 [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} i128


  ; Disallowed floating point types

  phi half [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} half

  phi x86_fp80 [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} x86_fp80

  phi fp128 [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} fp128

  phi ppc_fp128 [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} ppc_fp128

  phi x86_mmx [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} x86_mmx


  ; Derived types are disallowed too

  phi i32* [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} i32*

  phi [1 x i32] [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} [1 x i32]

  phi { i32, float } [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} { i32, float }

  phi void (i32)* [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} void (i32)*

  phi <{ i8, i32 }> [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} <{ i8, i32 }>

  ; Vector types are disallowed
  phi <2 x i32> [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} <2 x i32>

  ret void
}


; Named types. With the current implementation, named types are legal
; until they are actually attempted to be used. Might want to fix that.
%struct.s1 = type { half, float}
%struct.s2 = type { i32, i32}

define internal void @func2() {
entry:
  br label %block
block:

  phi %struct.s1 [ undef, %entry ]
; CHECK: disallowed: bad operand: {{.*}} %struct.s1

  phi %struct.s2 [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} %struct.s2

  ret void
}


; Circularities:  here to make sure the verifier doesn't crash or assert.

; This oddity is perfectly legal according to the IR and ABI verifiers.
; Might want to fix that. (good luck initializing one of these, though.)
%struct.snake = type { i32, %struct.tail }
%struct.tail = type { %struct.snake, i32 }

%struct.linked = type { i32, %struct.linked * }

define internal void @func3() {
entry:
  br label %block
block:

  phi %struct.snake [ undef, %entry ]
; CHECK: disallowed: bad operand: {{.*}} %struct.snake

  phi %struct.linked [ undef, %entry ]
; CHECK-NEXT: disallowed: bad operand: {{.*}} %struct.linked

  ret void
}
