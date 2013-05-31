; RUN: pnacl-abicheck < %s | FileCheck %s


; Allowed cases

@bytes = global [7 x i8] c"abcdefg"

@ptr_to_ptr = global i32 ptrtoint (i32* @ptr to i32)
@ptr_to_func = global i32 ptrtoint (void ()* @func to i32)

@compound = global <{ [3 x i8], i32 }>
    <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>

@ptr = global i32 ptrtoint ([7 x i8]* @bytes to i32)

@addend_ptr = global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)
@addend_negative = global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 -1)

@addend_array1 = global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 1)
@addend_array2 = global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 7)
@addend_array3 = global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 9)

@addend_struct1 = global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)
@addend_struct2 = global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)

; CHECK-NOT: disallowed


; Disallowed cases

@bad_external = external global [1 x i8]
; CHECK: Global variable bad_external has no initializer (disallowed)

@bad_int = global i32 0
; CHECK: Global variable bad_int has non-flattened initializer (disallowed): i32 0

@bad_size = global i64 ptrtoint ([7 x i8]* @bytes to i64)
; CHECK: Global variable bad_size has non-flattened initializer

; "null" is not allowed.
@bad_ptr = global i8* null
; CHECK: Global variable bad_ptr has non-flattened initializer

@bad_ptr2 = global i64 ptrtoint (i8* null to i64)
; CHECK: Global variable bad_ptr2 has non-flattened initializer

@bad_sub = global i32 sub (i32 ptrtoint (i32* @ptr to i32), i32 1)
; CHECK: Global variable bad_sub has non-flattened initializer

; i16 not allowed here.
@bad_compound = global <{ i32, i16 }>
    <{ i32 ptrtoint (void ()* @func to i32), i16 0 }>
; CHECK: Global variable bad_compound has non-flattened initializer

; The struct type must be packed.
@non_packed_struct = global { [3 x i8], i32 }
    { [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }
; CHECK: Global variable non_packed_struct has non-flattened initializer

; The struct type must be anonymous.
%struct = type <{ [3 x i8], i32 }>
@named_struct = global %struct
    <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
; CHECK: Global variable named_struct has non-flattened initializer


define void @func() {
  ret void
}
