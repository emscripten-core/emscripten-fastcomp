; RUN: pnacl-abicheck < %s | FileCheck %s

; This test checks that the PNaCl ABI verifier enforces the normal
; form introduced by the ReplacePtrsWithInts pass.


@var = global [4 x i8] c"xxxx"
@ptr = global i32 ptrtoint ([4 x i8]* @var to i32)

declare i8* @llvm.nacl.read.tp()


define internal void @pointer_arg(i8* %arg) {
  ret void
}
; CHECK: Function pointer_arg has disallowed type

define internal i8* @pointer_return() {
  unreachable
}
; CHECK-NEXT: Function pointer_return has disallowed type

define internal void @func() {
  ret void
}

define internal void @func_with_arg(i32 %arg) {
  ret void
}


define internal void @allowed_cases(i32 %arg) {
  inttoptr i32 123 to i8*

  ptrtoint [4 x i8]* @var to i32

  %alloc = alloca i8
  ptrtoint i8* %alloc to i32
  load i8* %alloc, align 1

  ; These instructions may use a NormalizedPtr, which may be a global.
  load i32* @ptr, align 1
  store i32 123, i32* @ptr, align 1

  ; A NormalizedPtr may be a bitcast.
  %ptr_bitcast = bitcast [4 x i8]* @var to i32*
  load i32* %ptr_bitcast, align 1

  ; A NormalizedPtr may be an inttoptr.
  %ptr_from_int = inttoptr i32 123 to i32*
  load i32* %ptr_from_int, align 1

  ; Check direct and indirect function calls.
  %func_as_int = ptrtoint void ()* @func to i32
  %func_ptr = inttoptr i32 %func_as_int to void ()*
  call void %func_ptr()
  call void @func()
  call void @func_with_arg(i32 123)

  ; Intrinsic calls may return pointers.
  %thread_ptr = call i8* @llvm.nacl.read.tp()
  ptrtoint i8* %thread_ptr to i32

  ; Bitcasts between non-pointers are not restricted
  bitcast i64 0 to double
  bitcast i32 0 to float

  ; ConstantInts and Arguments are allowed as operands.
  add i32 %arg, 123

  ret void
}
; CHECK-NOT: disallowed


define internal void @bad_cases() {
entry:
  ptrtoint [4 x i8]* @var to i16
; CHECK: Function bad_cases disallowed: non-i32 ptrtoint

  inttoptr i16 123 to i8*
; CHECK-NEXT: non-i32 inttoptr

  %a = alloca i32
; CHECK-NEXT: non-i8 alloca: %a
  %a2 = alloca [4 x i8]
; CHECK-NEXT: non-i8 alloca: %a2

  store i32 0, i32* null, align 1
; CHECK-NEXT: bad pointer

  store i32 0, i32* undef, align 1
; CHECK-NEXT: bad pointer

  %bc = bitcast i32* @ptr to i31*
; CHECK-NEXT: bad result type
  store i31 0, i31* %bc, align 1
; CHECK-NEXT: bad pointer

  ; Only one level of bitcasts is allowed.
  %b = bitcast i32* %a to i8*
  %c = bitcast i8* %b to i16*
; CHECK-NEXT: operand not InherentPtr

  br label %block
block:
  %phi1 = phi i8* [ undef, %entry ]
; CHECK-NEXT: bad operand: %phi1
  %phi2 = phi i32* [ undef, %entry ]
; CHECK-NEXT: bad operand: %phi2

  icmp eq i32* @ptr, @ptr
; CHECK-NEXT: bad operand: {{.*}} icmp
  icmp eq void ()* @func, @func
; CHECK-NEXT: bad operand: {{.*}} icmp
  icmp eq i31 0, 0
; CHECK-NEXT: bad operand: {{.*}} icmp

  call void null()
; CHECK-NEXT: bad function callee operand

  call void @func_with_arg(i32 ptrtoint (i32* @ptr to i32))
; CHECK-NEXT: bad operand

  ; Taking the address of an intrinsic is not allowed.
  ptrtoint i8* ()* @llvm.nacl.read.tp to i32
; CHECK-NEXT: operand not InherentPtr

  ret void
}

; CHECK-NOT: disallowed
