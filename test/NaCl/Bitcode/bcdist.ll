; Simple test to show that we don't break distribution counts.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer --order-blocks-by-id \
; RUN:              | FileCheck %s

@bytes7 = internal global [7 x i8] c"abcdefg"

@ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)

@ptr_to_func = internal global i32 ptrtoint (void ()* @func to i32)

@compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>

@ptr = internal global i32 ptrtoint ([7 x i8]* @bytes7 to i32)

@addend_ptr = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)

@addend_negative = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 -1)

@addend_array1 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 1)

@addend_array2 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 7)

@addend_array3 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 9)

@addend_struct1 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)

@addend_struct2 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)

@ptr_to_func_align = internal global i32 ptrtoint (void ()* @func to i32), align 8

@char = internal constant [1 x i8] c"0"

@short = internal constant [2 x i8] zeroinitializer

@bytes = internal global [4 x i8] c"abcd"

declare i32 @bar(i32)

define void @func() {
  ret void
}

define void @AllocCastSimple() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

define void @AllocCastSimpleReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

define void @AllocCastDelete() {
  %1 = alloca i8, i32 4, align 8
  %2 = alloca i8, i32 4, align 8
  ret void
}

define void @AllocCastOpt() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  store i32 %2, i32* %3, align 1
  ret void
}

define void @AllocBitcast(i32) {
  %2 = alloca i8, i32 4, align 8
  %3 = add i32 %0, 1
  %4 = ptrtoint i8* %2 to i32
  %5 = bitcast [4 x i8]* @bytes to i32*
  store i32 %4, i32* %5, align 1
  ret void
}

define void @StoreGlobal() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

define void @StoreGlobalCastsReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

define i32 @StoreGlobalCastPtr2Int() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret i32 0
}

define void @CastAddAlloca() {
  %1 = alloca i8, i32 4, align 8
  %2 = add i32 1, 2
  %3 = ptrtoint i8* %1 to i32
  %4 = add i32 %3, 2
  %5 = add i32 1, %3
  %6 = add i32 %3, %3
  ret void
}

define void @CastAddGlobal() {
  %1 = add i32 1, 2
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = add i32 %2, 2
  %4 = add i32 1, %2
  %5 = add i32 %2, %2
  ret void
}

define void @CastBinop() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = ptrtoint [4 x i8]* @bytes to i32
  %4 = sub i32 %2, %3
  %5 = mul i32 %2, %3
  %6 = udiv i32 %2, %3
  %7 = urem i32 %2, %3
  %8 = srem i32 %2, %3
  %9 = shl i32 %2, %3
  %10 = lshr i32 %2, %3
  %11 = ashr i32 %2, %3
  %12 = and i32 %2, %3
  %13 = or i32 %2, %3
  %14 = xor i32 %2, %3
  ret void
}

define void @TestSavedPtrToInt() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = add i32 %2, 0
  %4 = call i32 @bar(i32 %2)
  ret void
}

define void @CastSelect() {
  %1 = alloca i8, i32 4, align 8
  %2 = select i1 true, i32 1, i32 2
  %3 = ptrtoint i8* %1 to i32
  %4 = select i1 true, i32 %3, i32 2
  %5 = ptrtoint [4 x i8]* @bytes to i32
  %6 = select i1 true, i32 1, i32 %5
  %7 = select i1 true, i32 %3, i32 %5
  %8 = select i1 true, i32 %5, i32 %3
  ret void
}

define void @PhiBackwardRefs(i1) {
  %2 = alloca i8, i32 4, align 8
  %3 = alloca i8, i32 4, align 8
  br i1 %0, label %true, label %false

true:                                             ; preds = %1
  %4 = bitcast i8* %2 to i32*
  %5 = load i32* %4
  %6 = ptrtoint i8* %3 to i32
  br label %merge

false:                                            ; preds = %1
  %7 = bitcast i8* %2 to i32*
  %8 = load i32* %7
  %9 = ptrtoint i8* %3 to i32
  br label %merge

merge:                                            ; preds = %false, %true
  %10 = phi i32 [ %6, %true ], [ %9, %false ]
  %11 = phi i32 [ %5, %true ], [ %8, %false ]
  ret void
}

define void @PhiForwardRefs(i1) {
  br label %start

merge:                                            ; preds = %false, %true
  %2 = phi i32 [ %11, %true ], [ %11, %false ]
  %3 = phi i32 [ %5, %true ], [ %7, %false ]
  ret void

true:                                             ; preds = %start
  %4 = inttoptr i32 %9 to i32*
  %5 = load i32* %4
  br label %merge

false:                                            ; preds = %start
  %6 = inttoptr i32 %9 to i32*
  %7 = load i32* %6
  br label %merge

start:                                            ; preds = %1
  %8 = alloca i8, i32 4, align 8
  %9 = ptrtoint i8* %8 to i32
  %10 = alloca i8, i32 4, align 8
  %11 = ptrtoint i8* %10 to i32
  br i1 %0, label %true, label %false
}

define void @PhiMergeCast(i1) {
  %2 = alloca i8, i32 4, align 8
  %3 = alloca i8, i32 4, align 8
  br i1 %0, label %true, label %false

true:                                             ; preds = %1
  %4 = bitcast i8* %2 to i32*
  %5 = load i32* %4
  %6 = ptrtoint i8* %3 to i32
  %7 = add i32 %5, %6
  br label %merge

false:                                            ; preds = %1
  %8 = bitcast i8* %2 to i32*
  %9 = load i32* %8
  %10 = ptrtoint i8* %3 to i32
  br label %merge

merge:                                            ; preds = %false, %true
  %11 = phi i32 [ %6, %true ], [ %10, %false ]
  %12 = phi i32 [ %5, %true ], [ %9, %false ]
  ret void
}

define void @LongReachingCasts(i1) {
  %2 = alloca i8, i32 4, align 8
  br i1 %0, label %Split1, label %Split2

Split1:                                           ; preds = %1
  br i1 %0, label %b1, label %b2

Split2:                                           ; preds = %1
  br i1 %0, label %b3, label %b4

b1:                                               ; preds = %Split1
  %3 = ptrtoint i8* %2 to i32
  %4 = bitcast [4 x i8]* @bytes to i32*
  store i32 %3, i32* %4, align 1
  store i32 %3, i32* %4, align 1
  ret void

b2:                                               ; preds = %Split1
  %5 = ptrtoint i8* %2 to i32
  %6 = bitcast [4 x i8]* @bytes to i32*
  store i32 %5, i32* %6, align 1
  store i32 %5, i32* %6, align 1
  ret void

b3:                                               ; preds = %Split2
  %7 = ptrtoint i8* %2 to i32
  %8 = bitcast [4 x i8]* @bytes to i32*
  store i32 %7, i32* %8, align 1
  store i32 %7, i32* %8, align 1
  ret void

b4:                                               ; preds = %Split2
  %9 = ptrtoint i8* %2 to i32
  %10 = bitcast [4 x i8]* @bytes to i32*
  store i32 %9, i32* %10, align 1
  store i32 %9, i32* %10, align 1
  ret void
}

define void @SwitchVariable(i32) {
  switch i32 %0, label %l1 [
    i32 1, label %l2
    i32 2, label %l2
    i32 4, label %l3
    i32 5, label %l3
  ]
                                                  ; No predecessors!
  br label %end

l1:                                               ; preds = %1
  br label %end

l2:                                               ; preds = %1, %1
  br label %end

l3:                                               ; preds = %1, %1
  br label %end

end:                                              ; preds = %l3, %l2, %l1, %2
  ret void
}


; CHECK:      # Toplevel Blocks: 1

; CHECK:      Block Histogram (8 elements):

; CHECK:        %File   Count %Count    # Bits    Bits/Elmt Block
; CHECK-NEXT:  {{.*}}       1  {{.*}}   {{.*}}       {{.*}} BLOCKINFO_BLOCK
; CHECK-NEXT:  {{.*}}       1  {{.*}}   {{.*}}       {{.*}} MODULE_BLOCK

; CHECK:          Subblocks (5 elements):

; CHECK:             Count %Count Subblock
; CHECK-NEXT:           19  82.61 FUNCTION_BLOCK
; CHECK-NEXT:            1   4.35 GLOBALVAR_BLOCK
; CHECK-NEXT:            1   4.35 TYPE_BLOCK_ID
; CHECK-NEXT:            1   4.35 VALUE_SYMTAB
; CHECK-NEXT:            1   4.35 BLOCKINFO_BLOCK

; CHECK:          Record Histogram: (2 elements):

; CHECK:             Count %Count    # Bits    Bits/Elmt   % Abv Record Kind
; CHECK-NEXT:           20  95.24    {{.*}}       {{.*}}         FUNCTION
; CHECK-NEXT:            1   4.76    {{.*}}       {{.*}}         VERSION

; CHECK:        Block Histogram (continued)
; CHECK-NEXT:   %File   Count %Count    # Bits    Bits/Elmt Block
; CHECK-NEXT:  {{.*}}      17 {{.*}}    {{.*}}       {{.*}} CONSTANTS_BLOCK

; CHECK:          Record Histogram: (2 elements):

; CHECK:             Count %Count    # Bits    Bits/Elmt   % Abv Record Kind
; CHECK-NEXT:           26  59.09    {{.*}}       {{.*}}  {{.*}} INTEGER
; CHECK-NEXT:           18  40.91    {{.*}}       {{.*}}  {{.*}} SETTYPE

; CHECK:        Block Histogram (continued)
; CHECK-NEXT:   %File   Count %Count    # Bits    Bits/Elmt Block
; CHECK-NEXT:  {{.*}}      19 {{.*}}    {{.*}}       {{.*}} FUNCTION_BLOCK

; CHECK:          Subblocks (1 elements):

; CHECK:             Count %Count Subblock
; CHECK-NEXT:           17 100.00 CONSTANTS_BLOCK

; CHECK:          Record Histogram: (12 elements):

; CHECK:             Count %Count    # Bits    Bits/Elmt   % Abv Record Kind
; CHECK-NEXT:           22  15.83    {{.*}}       {{.*}}  {{.*}} INST_RET
; CHECK-NEXT:           22  15.83    {{.*}}       {{.*}}  {{.*}} INST_BINOP
; CHECK-NEXT:           20  14.39    {{.*}}       {{.*}}         INST_ALLOCA
; CHECK-NEXT:           19  13.67    {{.*}}       {{.*}}         DECLAREBLOCKS
; CHECK-NEXT:           17  12.23    {{.*}}       {{.*}}         INST_BR
; CHECK-NEXT:           16  11.51    {{.*}}       {{.*}}  {{.*}} INST_STORE
; CHECK-NEXT:            6   4.32    {{.*}}       {{.*}}  {{.*}} INST_LOAD
; CHECK-NEXT:            6   4.32    {{.*}}       {{.*}}         INST_PHI
; CHECK-NEXT:            5   3.60    {{.*}}       {{.*}}         INST_VSELECT
; CHECK-NEXT:            4   2.88    {{.*}}       {{.*}}  {{.*}} FORWARDTYPEREF
; CHECK-NEXT:            1   0.72    {{.*}}       {{.*}}         INST_CALL
; CHECK-NEXT:            1   0.72    {{.*}}       {{.*}}         INST_SWITCH

; CHECK:        Block Histogram (continued)
; CHECK-NEXT:   %File   Count %Count    # Bits    Bits/Elmt Block
; CHECK-NEXT:  {{.*}}       1 {{.*}}    {{.*}}       {{.*}} VALUE_SYMTAB

; CHECK:          Record Histogram: (1 elements):

; CHECK:             Count %Count    # Bits    Bits/Elmt   % Abv Record Kind
; CHECK-NEXT:           36 100.00    {{.*}}       {{.*}}  {{.*}} ENTRY

; CHECK:        Block Histogram (continued)
; CHECK-NEXT:   %File   Count %Count    # Bits    Bits/Elmt Block
; CHECK-NEXT:  {{.*}}       1 {{.*}}    {{.*}}       {{.*}} TYPE_BLOCK_ID

; CHECK:          Record Histogram: (4 elements):

; CHECK:             Count %Count    # Bits    Bits/Elmt   % Abv Record Kind
; CHECK-NEXT:            5  50.00    {{.*}}       {{.*}}  {{.*}} FUNCTION
; CHECK-NEXT:            3  30.00    {{.*}}       {{.*}}         INTEGER
; CHECK-NEXT:            1  10.00    {{.*}}       {{.*}}         VOID
; CHECK-NEXT:            1  10.00    {{.*}}       {{.*}}         NUMENTRY

; CHECK:        Block Histogram (continued)
; CHECK-NEXT:   %File   Count %Count    # Bits    Bits/Elmt Block
; CHECK-NEXT:  {{.*}}       1 {{.*}}    {{.*}}       {{.*}} GLOBALVAR_BLOCK

; CHECK:          Record Histogram: (6 elements):

; CHECK:             Count %Count    # Bits    Bits/Elmt   % Abv Record Kind
; CHECK-NEXT:           16  45.71    {{.*}}       {{.*}}  {{.*}} VAR
; CHECK-NEXT:           12  34.29    {{.*}}       {{.*}}  {{.*}} RELOC
; CHECK-NEXT:            4  11.43    {{.*}}       {{.*}}  {{.*}} DATA
; CHECK-NEXT:            1   2.86    {{.*}}       {{.*}}         COUNT
; CHECK-NEXT:            1   2.86    {{.*}}       {{.*}}  {{.*}} ZEROFILL
; CHECK-NEXT:            1   2.86    {{.*}}       {{.*}}  {{.*}} COMPOUND
