; RUN: opt -flatten-globals %s -S | FileCheck %s
; RUN: opt -flatten-globals %s -S | FileCheck %s -check-prefix=CLEANED

target datalayout = "p:32:32:32"


; Check simple cases

@var_i32 = global i32 258
; CHECK: @var_i32 = global [4 x i8] c"\02\01\00\00"
; CLEANED-NOT: global i32 258

@external_var = external global i32
; CHECK: @external_var = external global [4 x i8]

@zero_init = global i32 0
; CHECK: @zero_init = global [4 x i8] zeroinitializer

@big_zero_init = global [2000 x i8] zeroinitializer
; CHECK: @big_zero_init = global [2000 x i8] zeroinitializer

@null_ptr = global i32* null
; CHECK: @null_ptr = global [4 x i8] zeroinitializer

@undef_value = global i32 undef
; CHECK: @undef_value = global [4 x i8] zeroinitializer

%opaque = type opaque
@opaque_extern = external global %opaque
; CHECK: @opaque_extern = external global [0 x i8]


; Check various data types

@var_i1 = global i8 1
; CHECK: @var_i1 = global [1 x i8] c"\01"

@var_i8 = global i8 65
; CHECK: @var_i8 = global [1 x i8] c"A"

@var_i16 = global i16 258
; CHECK: @var_i16 = global [2 x i8] c"\02\01"

@var_i64 = global i64 72623859790382856
; CHECK: @var_i64 = global [8 x i8] c"\08\07\06\05\04\03\02\01"

@var_i128 = global i128 1339673755198158349044581307228491536
; CHECK: @var_i128 = global [16 x i8] c"\10\0F\0E\0D\0C\0B\0A\09\08\07\06\05\04\03\02\01"

; Check that padding bits come out as zero.
@var_i121 = global i121 1339673755198158349044581307228491536
; CHECK: @var_i121 = global [16 x i8] c"\10\0F\0E\0D\0C\0B\0A\09\08\07\06\05\04\03\02\01"

@var_double = global double 123.456
; CHECK: @var_double = global [8 x i8] c"w\BE\9F\1A/\DD^@"

@var_float = global float 123.0
; CHECK: @var_float = global [4 x i8] c"\00\00\F6B"


; Check aggregates

@padded_struct = global { i8, i8, i32 } { i8 65, i8 66, i32 258 }
; CHECK: @padded_struct = global [8 x i8] c"AB\00\00\02\01\00\00"

@packed_struct = global <{ i8, i8, i32 }> <{ i8 67, i8 68, i32 258 }>
; CHECK: @packed_struct = global [6 x i8] c"CD\02\01\00\00"

@i8_array = global [6 x i8] c"Hello\00"
; CHECK: @i8_array = global [6 x i8] c"Hello\00"

@i16_array = global [3 x i16] [ i16 1, i16 2, i16 3 ]
; CHECK: @i16_array = global [6 x i8] c"\01\00\02\00\03\00"

%s = type { i8, i8 }
@struct_array = global [2 x %s] [%s { i8 1, i8 2 }, %s { i8 3, i8 4 }]
; CHECK: @struct_array = global [4 x i8] c"\01\02\03\04"

@vector = global <2 x i32> <i32 259, i32 520>
; CHECK: @vector = global [8 x i8] c"\03\01\00\00\08\02\00\00"


; Check that various attributes are preserved

@constant_var = constant i32 259
; CHECK: @constant_var = constant [4 x i8] c"\03\01\00\00"

@weak_external_var = extern_weak global i32
; CHECK: @weak_external_var = extern_weak global [4 x i8]

@tls_var = external thread_local global i32
; CHECK: @tls_var = external thread_local global [4 x i8]

@aligned_var = global i32 260, align 8
; CHECK: @aligned_var = global [4 x i8] c"\04\01\00\00", align 8


; Check alignment handling

@implicit_alignment_i32 = global i32 zeroinitializer
; CHECK: @implicit_alignment_i32 = global [4 x i8] zeroinitializer, align 4

@implicit_alignment_double = global double zeroinitializer
; CHECK: @implicit_alignment_double = global [8 x i8] zeroinitializer, align 8

@implicit_alignment_vector = global <16 x i8> zeroinitializer
; CHECK: @implicit_alignment_vector = global [16 x i8] zeroinitializer, align 16

; FlattenGlobals is not allowed to increase the alignment of the
; variable when an explicit section is specified (although PNaCl does
; not support this attribute).
@lower_alignment_section = global i32 0, section "mysection", align 1
; CHECK: @lower_alignment_section = global [4 x i8] zeroinitializer, section "mysection", align 1

; FlattenGlobals could increase the alignment when no section is
; specified, but it does not.
@lower_alignment = global i32 0, align 1
; CHECK: @lower_alignment = global [4 x i8] zeroinitializer, align 1


; Check handling of global references

@var1 = external global i32
@var2 = external global i8

%ptrs1 = type { i32*, i8*, i32 }
@ptrs1 = global %ptrs1 { i32* @var1, i8* null, i32 259 }
; CHECK: @ptrs1 = global <{ i32, [8 x i8] }> <{ i32 ptrtoint ([4 x i8]* @var1 to i32), [8 x i8] c"\00\00\00\00\03\01\00\00" }>

%ptrs2 = type { i32, i32*, i8* }
@ptrs2 = global %ptrs2 { i32 259, i32* @var1, i8* @var2 }
; CHECK: @ptrs2 = global <{ [4 x i8], i32, i32 }> <{ [4 x i8] c"\03\01\00\00", i32 ptrtoint ([4 x i8]* @var1 to i32), i32 ptrtoint ([1 x i8]* @var2 to i32) }>

%ptrs3 = type { i32*, [3 x i8], i8* }
@ptrs3 = global %ptrs3 { i32* @var1, [3 x i8] c"foo", i8* @var2 }
; CHECK: @ptrs3 = global <{ i32, [4 x i8], i32 }> <{ i32 ptrtoint ([4 x i8]* @var1 to i32), [4 x i8] c"foo\00", i32 ptrtoint ([1 x i8]* @var2 to i32) }>

@ptr = global i32* @var1
; CHECK: @ptr = global i32 ptrtoint ([4 x i8]* @var1 to i32)

@func_ptr = global i32* ()* @get_address
; CHECK: @func_ptr = global i32 ptrtoint (i32* ()* @get_address to i32)

@block_addr = global i8* blockaddress(@func_with_block, %label)
; CHECK: @block_addr = global i32 ptrtoint (i8* blockaddress(@func_with_block, %label) to i32)

@vector_reloc = global <2 x i32*> <i32* @var1, i32* @var1>
; CHECK: global <{ i32, i32 }> <{ i32 ptrtoint ([4 x i8]* @var1 to i32), i32 ptrtoint ([4 x i8]* @var1 to i32) }>


; Global references with addends

@reloc_addend = global i32* getelementptr (%ptrs1, %ptrs1* @ptrs1, i32 0, i32 2)
; CHECK: @reloc_addend = global i32 add (i32 ptrtoint (<{ i32, [8 x i8] }>* @ptrs1 to i32), i32 8)

@negative_addend = global %ptrs1* getelementptr (%ptrs1, %ptrs1* @ptrs1, i32 -1)
; CHECK: @negative_addend = global i32 add (i32 ptrtoint (<{ i32, [8 x i8] }>* @ptrs1 to i32), i32 -12)

@const_ptr = global i32* getelementptr (%ptrs1, %ptrs1* null, i32 0, i32 2)
; CHECK: @const_ptr = global [4 x i8] c"\08\00\00\00"

@int_to_ptr = global i32* inttoptr (i16 260 to i32*)
; CHECK: @int_to_ptr = global [4 x i8] c"\04\01\00\00"

; Clang allows "(uintptr_t) &var" as a global initializer, so we
; handle this case.
@ptr_to_int = global i32 ptrtoint (i8* @var2 to i32)
; CHECK: @ptr_to_int = global i32 ptrtoint ([1 x i8]* @var2 to i32)

; This is handled via Constant folding.  The getelementptr is
; converted to an undef when it is created, so the pass does not see a
; getelementptr here.
@undef_gep = global i32* getelementptr (%ptrs1, %ptrs1* undef, i32 0, i32 2)
; CHECK: @undef_gep = global [4 x i8] zeroinitializer

; Adding an offset to a function address isn't useful, but check that
; the pass handles it anyway.
@func_addend = global i8* getelementptr (
    i8, 
    i8* bitcast (void ()* @func_with_block to i8*), i32 123)
; CHECK: @func_addend = global i32 add (i32 ptrtoint (void ()* @func_with_block to i32), i32 123)

; Similarly, adding an offset to a label address isn't useful, but
; check it anyway.
@block_addend = global i8* getelementptr (
    i8, 
    i8* blockaddress(@func_with_block, %label), i32 100)
; CHECK: @block_addend = global i32 add (i32 ptrtoint (i8* blockaddress(@func_with_block, %label) to i32), i32 100)


; Special cases

; Leave vars with "appending" linkage alone.
@appending = appending global [1 x i32*] [i32* @var1]
; CHECK: @appending = appending global [1 x i32*] [i32* bitcast ([4 x i8]* @var1 to i32*)]


define i32* @get_address() {
  ret i32* @var_i32
}
; CHECK: define i32* @get_address() {
; CHECK-NEXT: ret i32* bitcast ([4 x i8]* @var_i32 to i32*)


define void @func_with_block() {
  br label %label
label:
  ret void
}
