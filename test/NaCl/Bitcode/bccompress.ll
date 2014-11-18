; Simple test to see if pnacl-bccompress maintains bitcode.

; Test 1: Show that we generate the same disassembled code.
; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-bccompress \
; RUN:              | pnacl-thaw -allow-local-symbol-tables \
; RUN:              | llvm-dis - | FileCheck %s

; Test 2: Show that both the precompressed, and the compressed versions
; of the bitcode contain the same records.
; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer -operands-per-line=6 -dump-records \
; RUN:              | FileCheck %s --check-prefix DUMP
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress \
; RUN:              | pnacl-bcanalyzer -operands-per-line=6 -dump-records \
; RUN:              | FileCheck %s --check-prefix DUMP

@bytes7 = internal global [7 x i8] c"abcdefg"
; CHECK: @bytes7 = internal global [7 x i8] c"abcdefg"

@ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)
; CHECK-NEXT: @ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)

@ptr_to_func = internal global i32 ptrtoint (void ()* @func to i32)
; CHECK-NEXT: @ptr_to_func = internal global i32 ptrtoint (void ()* @func to i32)

@compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
; CHECK-NEXT: @compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>

@ptr = internal global i32 ptrtoint ([7 x i8]* @bytes7 to i32)
; CHECK-NEXT: @ptr = internal global i32 ptrtoint ([7 x i8]* @bytes7 to i32)

@addend_ptr = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)
; CHECK-NEXT: @addend_ptr = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)

@addend_negative = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 -1)
; CHECK-NEXT: @addend_negative = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 -1)

@addend_array1 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 1)
; CHECK-NEXT: @addend_array1 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 1)

@addend_array2 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 7)
; CHECK-NEXT: @addend_array2 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 7)

@addend_array3 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 9)
; CHECK-NEXT: @addend_array3 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes7 to i32), i32 9)

@addend_struct1 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)
; CHECK-NEXT: @addend_struct1 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)

@addend_struct2 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)
; CHECK-NEXT: @addend_struct2 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)

@ptr_to_func_align = internal global i32 ptrtoint (void ()* @func to i32), align 8
; CHECK-NEXT: @ptr_to_func_align = internal global i32 ptrtoint (void ()* @func to i32), align 8

@char = internal constant [1 x i8] c"0"
; CHECK-NEXT: @char = internal constant [1 x i8] c"0"

@short = internal constant [2 x i8] zeroinitializer
; CHECK-NEXT: @short = internal constant [2 x i8] zeroinitializer

@bytes = internal global [4 x i8] c"abcd"
; CHECK-NEXT: @bytes = internal global [4 x i8] c"abcd"

declare i32 @bar(i32)
; CHECK: declare i32 @bar(i32)

define void @func() {
  ret void
}

; CHECK:      define void @func() {
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @AllocCastSimple() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

; CHECK:      define void @AllocCastSimple() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @AllocCastSimpleReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

; CHECK:      define void @AllocCastSimpleReversed() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @AllocCastDelete() {
  %1 = alloca i8, i32 4, align 8
  %2 = alloca i8, i32 4, align 8
  ret void
}

; CHECK:      define void @AllocCastDelete() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = alloca i8, i32 4, align 8
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @AllocCastOpt() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  store i32 %2, i32* %3, align 1
  ret void
}

; CHECK:      define void @AllocCastOpt() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @AllocBitcast(i32) {
  %2 = alloca i8, i32 4, align 8
  %3 = add i32 %0, 1
  %4 = ptrtoint i8* %2 to i32
  %5 = bitcast [4 x i8]* @bytes to i32*
  store i32 %4, i32* %5, align 1
  ret void
}

; CHECK:      define void @AllocBitcast(i32) {
; CHECK-NEXT:   %2 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %3 = add i32 %0, 1
; CHECK-NEXT:   %4 = ptrtoint i8* %2 to i32
; CHECK-NEXT:   %5 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %4, i32* %5, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @StoreGlobal() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

; CHECK:      define void @StoreGlobal() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; CHECK-NEXT:   %3 = bitcast i8* %1 to i32*
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @StoreGlobalCastsReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

; CHECK:      define void @StoreGlobalCastsReversed() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; CHECK-NEXT:   %3 = bitcast i8* %1 to i32*
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define i32 @StoreGlobalCastPtr2Int() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret i32 0
}

; CHECK:      define i32 @StoreGlobalCastPtr2Int() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; CHECK-NEXT:   %3 = bitcast i8* %1 to i32*
; CHECK-NEXT:   store i32 %2, i32* %3, align 1
; CHECK-NEXT:   ret i32 0
; CHECK-NEXT: }

define void @CastAddAlloca() {
  %1 = alloca i8, i32 4, align 8
  %2 = add i32 1, 2
  %3 = ptrtoint i8* %1 to i32
  %4 = add i32 %3, 2
  %5 = add i32 1, %3
  %6 = add i32 %3, %3
  ret void
}

; CHECK:      define void @CastAddAlloca() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = add i32 1, 2
; CHECK-NEXT:   %3 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %4 = add i32 %3, 2
; CHECK-NEXT:   %5 = add i32 1, %3
; CHECK-NEXT:   %6 = add i32 %3, %3
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @CastAddGlobal() {
  %1 = add i32 1, 2
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = add i32 %2, 2
  %4 = add i32 1, %2
  %5 = add i32 %2, %2
  ret void
}

; CHECK:      define void @CastAddGlobal() {
; CHECK-NEXT:   %1 = add i32 1, 2
; CHECK-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; CHECK-NEXT:   %3 = add i32 %2, 2
; CHECK-NEXT:   %4 = add i32 1, %2
; CHECK-NEXT:   %5 = add i32 %2, %2
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

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

; CHECK:      define void @CastBinop() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; CHECK-NEXT:   %4 = sub i32 %2, %3
; CHECK-NEXT:   %5 = mul i32 %2, %3
; CHECK-NEXT:   %6 = udiv i32 %2, %3
; CHECK-NEXT:   %7 = urem i32 %2, %3
; CHECK-NEXT:   %8 = srem i32 %2, %3
; CHECK-NEXT:   %9 = shl i32 %2, %3
; CHECK-NEXT:   %10 = lshr i32 %2, %3
; CHECK-NEXT:   %11 = ashr i32 %2, %3
; CHECK-NEXT:   %12 = and i32 %2, %3
; CHECK-NEXT:   %13 = or i32 %2, %3
; CHECK-NEXT:   %14 = xor i32 %2, %3
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

define void @TestSavedPtrToInt() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = add i32 %2, 0
  %4 = call i32 @bar(i32 %2)
  ret void
}

; CHECK:      define void @TestSavedPtrToInt() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %3 = add i32 %2, 0
; CHECK-NEXT:   %4 = call i32 @bar(i32 %2)
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

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

; CHECK:      define void @CastSelect() {
; CHECK-NEXT:   %1 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %2 = select i1 true, i32 1, i32 2
; CHECK-NEXT:   %3 = ptrtoint i8* %1 to i32
; CHECK-NEXT:   %4 = select i1 true, i32 %3, i32 2
; CHECK-NEXT:   %5 = ptrtoint [4 x i8]* @bytes to i32
; CHECK-NEXT:   %6 = select i1 true, i32 1, i32 %5
; CHECK-NEXT:   %7 = select i1 true, i32 %3, i32 %5
; CHECK-NEXT:   %8 = select i1 true, i32 %5, i32 %3
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

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

; CHECK:      define void @PhiBackwardRefs(i1) {
; CHECK-NEXT:   %2 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %3 = alloca i8, i32 4, align 8
; CHECK-NEXT:   br i1 %0, label %true, label %false
; CHECK:      true:                                             ; preds = %1
; CHECK-NEXT:   %4 = bitcast i8* %2 to i32*
; CHECK-NEXT:   %5 = load i32* %4
; CHECK-NEXT:   %6 = ptrtoint i8* %3 to i32
; CHECK-NEXT:   br label %merge
; CHECK:      false:                                            ; preds = %1
; CHECK-NEXT:   %7 = bitcast i8* %2 to i32*
; CHECK-NEXT:   %8 = load i32* %7
; CHECK-NEXT:   %9 = ptrtoint i8* %3 to i32
; CHECK-NEXT:   br label %merge
; CHECK:      merge:                                            ; preds = %false, %true
; CHECK-NEXT:   %10 = phi i32 [ %6, %true ], [ %9, %false ]
; CHECK-NEXT:   %11 = phi i32 [ %5, %true ], [ %8, %false ]
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

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

; CHECK:      define void @PhiForwardRefs(i1) {
; CHECK-NEXT:   br label %start
; CHECK:      merge:                                            ; preds = %false, %true
; CHECK-NEXT:   %2 = phi i32 [ %11, %true ], [ %11, %false ]
; CHECK-NEXT:   %3 = phi i32 [ %5, %true ], [ %7, %false ]
; CHECK-NEXT:   ret void
; CHECK:      true:                                             ; preds = %start
; CHECK-NEXT:   %4 = inttoptr i32 %9 to i32*
; CHECK-NEXT:   %5 = load i32* %4
; CHECK-NEXT:   br label %merge
; CHECK:      false:                                            ; preds = %start
; CHECK-NEXT:   %6 = inttoptr i32 %9 to i32*
; CHECK-NEXT:   %7 = load i32* %6
; CHECK-NEXT:   br label %merge
; CHECK:      start:                                            ; preds = %1
; CHECK-NEXT:   %8 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %9 = ptrtoint i8* %8 to i32
; CHECK-NEXT:   %10 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %11 = ptrtoint i8* %10 to i32
; CHECK-NEXT:   br i1 %0, label %true, label %false
; CHECK-NEXT: }

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

; CHECK:      define void @PhiMergeCast(i1) {
; CHECK-NEXT:   %2 = alloca i8, i32 4, align 8
; CHECK-NEXT:   %3 = alloca i8, i32 4, align 8
; CHECK-NEXT:   br i1 %0, label %true, label %false
; CHECK:      true:                                             ; preds = %1
; CHECK-NEXT:   %4 = bitcast i8* %2 to i32*
; CHECK-NEXT:   %5 = load i32* %4
; CHECK-NEXT:   %6 = ptrtoint i8* %3 to i32
; CHECK-NEXT:   %7 = add i32 %5, %6
; CHECK-NEXT:   br label %merge
; CHECK:      false:                                            ; preds = %1
; CHECK-NEXT:   %8 = bitcast i8* %2 to i32*
; CHECK-NEXT:   %9 = load i32* %8
; CHECK-NEXT:   %10 = ptrtoint i8* %3 to i32
; CHECK-NEXT:   br label %merge
; CHECK:      merge:                                            ; preds = %false, %true
; CHECK-NEXT:   %11 = phi i32 [ %6, %true ], [ %10, %false ]
; CHECK-NEXT:   %12 = phi i32 [ %5, %true ], [ %9, %false ]
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

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

; CHECK:      define void @LongReachingCasts(i1) {
; CHECK-NEXT:   %2 = alloca i8, i32 4, align 8
; CHECK-NEXT:   br i1 %0, label %Split1, label %Split2
; CHECK:      Split1:                                           ; preds = %1
; CHECK-NEXT:   br i1 %0, label %b1, label %b2
; CHECK:      Split2:                                           ; preds = %1
; CHECK-NEXT:   br i1 %0, label %b3, label %b4
; CHECK:      b1:                                               ; preds = %Split1
; CHECK-NEXT:   %3 = ptrtoint i8* %2 to i32
; CHECK-NEXT:   %4 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %3, i32* %4, align 1
; CHECK-NEXT:   store i32 %3, i32* %4, align 1
; CHECK-NEXT:   ret void
; CHECK:      b2:                                               ; preds = %Split1
; CHECK-NEXT:   %5 = ptrtoint i8* %2 to i32
; CHECK-NEXT:   %6 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %5, i32* %6, align 1
; CHECK-NEXT:   store i32 %5, i32* %6, align 1
; CHECK-NEXT:   ret void
; CHECK:      b3:                                               ; preds = %Split2
; CHECK-NEXT:   %7 = ptrtoint i8* %2 to i32
; CHECK-NEXT:   %8 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %7, i32* %8, align 1
; CHECK-NEXT:   store i32 %7, i32* %8, align 1
; CHECK-NEXT:   ret void
; CHECK:      b4:                                               ; preds = %Split2
; CHECK-NEXT:   %9 = ptrtoint i8* %2 to i32
; CHECK-NEXT:   %10 = bitcast [4 x i8]* @bytes to i32*
; CHECK-NEXT:   store i32 %9, i32* %10, align 1
; CHECK-NEXT:   store i32 %9, i32* %10, align 1
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

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

; CHECK:      define void @SwitchVariable(i32) {
; CHECK-NEXT:   switch i32 %0, label %l1 [
; CHECK-NEXT:     i32 1, label %l2
; CHECK-NEXT:     i32 2, label %l2
; CHECK-NEXT:     i32 4, label %l3
; CHECK-NEXT:     i32 5, label %l3
; CHECK-NEXT:   ]
; CHECK-NEXT:                                                   ; No predecessors!
; CHECK-NEXT:   br label %end
; CHECK:      l1:                                               ; preds = %1
; CHECK-NEXT:   br label %end
; CHECK:      l2:                                               ; preds = %1, %1
; CHECK-NEXT:   br label %end
; CHECK:      l3:                                               ; preds = %1, %1
; CHECK-NEXT:   br label %end
; CHECK:      end:                                              ; preds = %l3, %l2, %l1, %2
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

; DUMP:     PNaCl Version: 2

; DUMP:      <MODULE_BLOCK>
; DUMP-NEXT:  <VERSION op0=1/>
; DUMP-NEXT:  <BLOCKINFO_BLOCK/>
; DUMP-NEXT:  <TYPE_BLOCK_ID>
; DUMP-NEXT:    <NUMENTRY op0=9/>
; DUMP-NEXT:    <INTEGER op0=32/>
; DUMP-NEXT:    <VOID/>
; DUMP-NEXT:    <INTEGER op0=8/>
; DUMP-NEXT:    <INTEGER op0=1/>
; DUMP-NEXT:    <FUNCTION op0=0 op1=1/>
; DUMP-NEXT:    <FUNCTION op0=0 op1=1 op2=3/>
; DUMP-NEXT:    <FUNCTION op0=0 op1=0 op2=0/>
; DUMP-NEXT:    <FUNCTION op0=0 op1=1 op2=0/>
; DUMP-NEXT:    <FUNCTION op0=0 op1=0/>
; DUMP-NEXT:  </TYPE_BLOCK_ID>
; DUMP-NEXT:  <FUNCTION op0=6 op1=0 op2=1 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=7 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=8 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=4 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=5 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=5 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=5 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=5 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <FUNCTION op0=7 op1=0 op2=0 op3=0/>
; DUMP-NEXT:  <GLOBALVAR_BLOCK>
; DUMP-NEXT:    <COUNT op0=16/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <DATA op0=97 op1=98 op2=99 op3=100 op4=101 op5=102
; DUMP-NEXT:            op6=103/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=24/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=1/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <COMPOUND op0=2/>
; DUMP-NEXT:    <DATA op0=102 op1=111 op2=111/>
; DUMP-NEXT:    <RELOC op0=1/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=20/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=24 op1=1/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=24 op1=4294967295/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=20 op1=1/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=20 op1=7/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=20 op1=9/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=23 op1=1/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <RELOC op0=23 op1=4/>
; DUMP-NEXT:    <VAR op0=4 op1=0/>
; DUMP-NEXT:    <RELOC op0=1/>
; DUMP-NEXT:    <VAR op0=0 op1=1/>
; DUMP-NEXT:    <DATA op0=48/>
; DUMP-NEXT:    <VAR op0=0 op1=1/>
; DUMP-NEXT:    <ZEROFILL op0=2/>
; DUMP-NEXT:    <VAR op0=0 op1=0/>
; DUMP-NEXT:    <DATA op0=97 op1=98 op2=99 op3=100/>
; DUMP-NEXT:  </GLOBALVAR_BLOCK>
; DUMP-NEXT:  <VALUE_SYMTAB>
; DUMP-NEXT:    <ENTRY op0=2 op1=65 op2=108 op3=108 op4=111 op5=99
; DUMP-NEXT:             op6=67 op7=97 op8=115 op9=116 op10=83 op11=105
; DUMP-NEXT:             op12=109 op13=112 op14=108 op15=101/>
; DUMP-NEXT:    <ENTRY op0=10 op1=67 op2=97 op3=115 op4=116 op5=65
; DUMP-NEXT:             op6=100 op7=100 op8=65 op9=108 op10=108 op11=111
; DUMP-NEXT:             op12=99 op13=97/>
; DUMP-NEXT:    <ENTRY op0=20 op1=98 op2=121 op3=116 op4=101 op5=115
; DUMP-NEXT:             op6=55/>
; DUMP-NEXT:    <ENTRY op0=23 op1=99 op2=111 op3=109 op4=112 op5=111
; DUMP-NEXT:             op6=117 op7=110 op8=100/>
; DUMP-NEXT:    <ENTRY op0=5 op1=65 op2=108 op3=108 op4=111 op5=99
; DUMP-NEXT:             op6=67 op7=97 op8=115 op9=116 op10=79 op11=112
; DUMP-NEXT:             op12=116/>
; DUMP-NEXT:    <ENTRY op0=32 op1=112 op2=116 op3=114 op4=95 op5=116
; DUMP-NEXT:             op6=111 op7=95 op8=102 op9=117 op10=110 op11=99
; DUMP-NEXT:             op12=95 op13=97 op14=108 op15=105 op16=103 op17=110/>
; DUMP-NEXT:    <ENTRY op0=18 op1=76 op2=111 op3=110 op4=103 op5=82
; DUMP-NEXT:             op6=101 op7=97 op8=99 op9=104 op10=105 op11=110
; DUMP-NEXT:             op12=103 op13=67 op14=97 op15=115 op16=116 op17=115/>
; DUMP-NEXT:    <ENTRY op0=15 op1=80 op2=104 op3=105 op4=66 op5=97
; DUMP-NEXT:             op6=99 op7=107 op8=119 op9=97 op10=114 op11=100
; DUMP-NEXT:             op12=82 op13=101 op14=102 op15=115/>
; DUMP-NEXT:    <ENTRY op0=26 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=110 op9=101 op10=103 op11=97
; DUMP-NEXT:             op12=116 op13=105 op14=118 op15=101/>
; DUMP-NEXT:    <ENTRY op0=6 op1=65 op2=108 op3=108 op4=111 op5=99
; DUMP-NEXT:             op6=66 op7=105 op8=116 op9=99 op10=97 op11=115
; DUMP-NEXT:             op12=116/>
; DUMP-NEXT:    <ENTRY op0=24 op1=112 op2=116 op3=114/>
; DUMP-NEXT:    <ENTRY op0=25 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=112 op9=116 op10=114/>
; DUMP-NEXT:    <ENTRY op0=19 op1=83 op2=119 op3=105 op4=116 op5=99
; DUMP-NEXT:             op6=104 op7=86 op8=97 op9=114 op10=105 op11=97
; DUMP-NEXT:             op12=98 op13=108 op14=101/>
; DUMP-NEXT:    <ENTRY op0=0 op1=98 op2=97 op3=114/>
; DUMP-NEXT:    <ENTRY op0=33 op1=99 op2=104 op3=97 op4=114/>
; DUMP-NEXT:    <ENTRY op0=3 op1=65 op2=108 op3=108 op4=111 op5=99
; DUMP-NEXT:             op6=67 op7=97 op8=115 op9=116 op10=83 op11=105
; DUMP-NEXT:             op12=109 op13=112 op14=108 op15=101 op16=82 op17=101
; DUMP-NEXT:             op18=118 op19=101 op20=114 op21=115 op22=101 op23=100/>
; DUMP-NEXT:    <ENTRY op0=22 op1=112 op2=116 op3=114 op4=95 op5=116
; DUMP-NEXT:             op6=111 op7=95 op8=102 op9=117 op10=110 op11=99/>
; DUMP-NEXT:    <ENTRY op0=12 op1=67 op2=97 op3=115 op4=116 op5=66
; DUMP-NEXT:             op6=105 op7=110 op8=111 op9=112/>
; DUMP-NEXT:    <ENTRY op0=11 op1=67 op2=97 op3=115 op4=116 op5=65
; DUMP-NEXT:             op6=100 op7=100 op8=71 op9=108 op10=111 op11=98
; DUMP-NEXT:             op12=97 op13=108/>
; DUMP-NEXT:    <ENTRY op0=16 op1=80 op2=104 op3=105 op4=70 op5=111
; DUMP-NEXT:             op6=114 op7=119 op8=97 op9=114 op10=100 op11=82
; DUMP-NEXT:             op12=101 op13=102 op14=115/>
; DUMP-NEXT:    <ENTRY op0=35 op1=98 op2=121 op3=116 op4=101 op5=115/>
; DUMP-NEXT:    <ENTRY op0=4 op1=65 op2=108 op3=108 op4=111 op5=99
; DUMP-NEXT:             op6=67 op7=97 op8=115 op9=116 op10=68 op11=101
; DUMP-NEXT:             op12=108 op13=101 op14=116 op15=101/>
; DUMP-NEXT:    <ENTRY op0=14 op1=67 op2=97 op3=115 op4=116 op5=83
; DUMP-NEXT:             op6=101 op7=108 op8=101 op9=99 op10=116/>
; DUMP-NEXT:    <ENTRY op0=1 op1=102 op2=117 op3=110 op4=99/>
; DUMP-NEXT:    <ENTRY op0=21 op1=112 op2=116 op3=114 op4=95 op5=116
; DUMP-NEXT:             op6=111 op7=95 op8=112 op9=116 op10=114/>
; DUMP-NEXT:    <ENTRY op0=27 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=97 op9=114 op10=114 op11=97
; DUMP-NEXT:             op12=121 op13=49/>
; DUMP-NEXT:    <ENTRY op0=28 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=97 op9=114 op10=114 op11=97
; DUMP-NEXT:             op12=121 op13=50/>
; DUMP-NEXT:    <ENTRY op0=29 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=97 op9=114 op10=114 op11=97
; DUMP-NEXT:             op12=121 op13=51/>
; DUMP-NEXT:    <ENTRY op0=34 op1=115 op2=104 op3=111 op4=114 op5=116/>
; DUMP-NEXT:    <ENTRY op0=30 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=115 op9=116 op10=114 op11=117
; DUMP-NEXT:             op12=99 op13=116 op14=49/>
; DUMP-NEXT:    <ENTRY op0=31 op1=97 op2=100 op3=100 op4=101 op5=110
; DUMP-NEXT:             op6=100 op7=95 op8=115 op9=116 op10=114 op11=117
; DUMP-NEXT:             op12=99 op13=116 op14=50/>
; DUMP-NEXT:    <ENTRY op0=13 op1=84 op2=101 op3=115 op4=116 op5=83
; DUMP-NEXT:             op6=97 op7=118 op8=101 op9=100 op10=80 op11=116
; DUMP-NEXT:             op12=114 op13=84 op14=111 op15=73 op16=110 op17=116/>
; DUMP-NEXT:    <ENTRY op0=17 op1=80 op2=104 op3=105 op4=77 op5=101
; DUMP-NEXT:             op6=114 op7=103 op8=101 op9=67 op10=97 op11=115
; DUMP-NEXT:             op12=116/>
; DUMP-NEXT:    <ENTRY op0=8 op1=83 op2=116 op3=111 op4=114 op5=101
; DUMP-NEXT:             op6=71 op7=108 op8=111 op9=98 op10=97 op11=108
; DUMP-NEXT:             op12=67 op13=97 op14=115 op15=116 op16=115 op17=82
; DUMP-NEXT:             op18=101 op19=118 op20=101 op21=114 op22=115 op23=101
; DUMP-NEXT:             op24=100/>
; DUMP-NEXT:    <ENTRY op0=7 op1=83 op2=116 op3=111 op4=114 op5=101
; DUMP-NEXT:             op6=71 op7=108 op8=111 op9=98 op10=97 op11=108/>
; DUMP-NEXT:    <ENTRY op0=9 op1=83 op2=116 op3=111 op4=114 op5=101
; DUMP-NEXT:             op6=71 op7=108 op8=111 op9=98 op10=97 op11=108
; DUMP-NEXT:             op12=67 op13=97 op14=115 op15=116 op16=80 op17=116
; DUMP-NEXT:             op18=114 op19=50 op20=73 op21=110 op22=116/>
; DUMP-NEXT:  </VALUE_SYMTAB>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_STORE op0=3 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_STORE op0=3 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_STORE op0=3 op1=1 op2=1/>
; DUMP-NEXT:    <INST_STORE op0=3 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:      <INTEGER op0=2/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_BINOP op0=4 op1=2 op2=0/>
; DUMP-NEXT:    <INST_STORE op0=6 op1=2 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_STORE op0=1 op1=3 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_STORE op0=1 op1=3 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:      <INTEGER op0=0/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_STORE op0=1 op1=4 op2=1/>
; DUMP-NEXT:    <INST_RET op0=2/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=2/>
; DUMP-NEXT:      <INTEGER op0=4/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_BINOP op0=4 op1=3 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=2 op1=4 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=6 op1=3 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=4 op1=4 op2=0/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=2/>
; DUMP-NEXT:      <INTEGER op0=4/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_BINOP op0=2 op1=1 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=4 op1=2 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=4 op1=5 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=6 op1=6 op2=0/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_BINOP op0=1 op1=3 op2=1/>
; DUMP-NEXT:    <INST_BINOP op0=2 op1=4 op2=2/>
; DUMP-NEXT:    <INST_BINOP op0=3 op1=5 op2=3/>
; DUMP-NEXT:    <INST_BINOP op0=4 op1=6 op2=5/>
; DUMP-NEXT:    <INST_BINOP op0=5 op1=7 op2=6/>
; DUMP-NEXT:    <INST_BINOP op0=6 op1=8 op2=7/>
; DUMP-NEXT:    <INST_BINOP op0=7 op1=9 op2=8/>
; DUMP-NEXT:    <INST_BINOP op0=8 op1=10 op2=9/>
; DUMP-NEXT:    <INST_BINOP op0=9 op1=11 op2=10/>
; DUMP-NEXT:    <INST_BINOP op0=10 op1=12 op2=11/>
; DUMP-NEXT:    <INST_BINOP op0=11 op1=13 op2=12/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:      <INTEGER op0=0/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_BINOP op0=1 op1=2 op2=0/>
; DUMP-NEXT:    <INST_CALL op0=0 op1=40 op2=2/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=1/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=2/>
; DUMP-NEXT:      <INTEGER op0=4/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:      <SETTYPE op0=3/>
; DUMP-NEXT:      <INTEGER op0=3/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_VSELECT op0=5 op1=4 op2=2/>
; DUMP-NEXT:    <INST_VSELECT op0=2 op1=5 op2=3/>
; DUMP-NEXT:    <INST_VSELECT op0=7 op1=8 op2=4/>
; DUMP-NEXT:    <INST_VSELECT op0=4 op1=9 op2=5/>
; DUMP-NEXT:    <INST_VSELECT op0=10 op1=5 op2=6/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=4/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_BR op0=1 op1=2 op2=4/>
; DUMP-NEXT:    <INST_LOAD op0=2 op1=0 op2=0/>
; DUMP-NEXT:    <INST_BR op0=3/>
; DUMP-NEXT:    <INST_LOAD op0=3 op1=0 op2=0/>
; DUMP-NEXT:    <INST_BR op0=3/>
; DUMP-NEXT:    <INST_PHI op0=0 op1=6 op2=1 op3=6 op4=2/>
; DUMP-NEXT:    <INST_PHI op0=0 op1=6 op2=1 op3=4 op4=2/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=5/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_BR op0=4/>
; DUMP-NEXT:    <FORWARDTYPEREF op0=43 op1=0/>
; DUMP-NEXT:    <INST_PHI op0=0 op1=11 op2=2 op3=11 op4=3/>
; DUMP-NEXT:    <FORWARDTYPEREF op0=40 op1=0/>
; DUMP-NEXT:    <FORWARDTYPEREF op0=41 op1=0/>
; DUMP-NEXT:    <INST_PHI op0=0 op1=3 op2=2 op3=5 op4=3/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:    <FORWARDTYPEREF op0=42 op1=0/>
; DUMP-NEXT:    <INST_LOAD op0=4294967294 op1=0 op2=0/>
; DUMP-NEXT:    <INST_BR op0=1/>
; DUMP-NEXT:    <INST_LOAD op0=4294967295 op1=0 op2=0/>
; DUMP-NEXT:    <INST_BR op0=1/>
; DUMP-NEXT:    <INST_ALLOCA op0=5 op1=4/>
; DUMP-NEXT:    <INST_ALLOCA op0=6 op1=4/>
; DUMP-NEXT:    <INST_BR op0=2 op1=3 op2=8/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=4/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_ALLOCA op0=2 op1=4/>
; DUMP-NEXT:    <INST_BR op0=1 op1=2 op2=4/>
; DUMP-NEXT:    <INST_LOAD op0=2 op1=0 op2=0/>
; DUMP-NEXT:    <INST_BINOP op0=1 op1=2 op2=0/>
; DUMP-NEXT:    <INST_BR op0=3/>
; DUMP-NEXT:    <INST_LOAD op0=4 op1=0 op2=0/>
; DUMP-NEXT:    <INST_BR op0=3/>
; DUMP-NEXT:    <INST_PHI op0=0 op1=8 op2=1 op3=8 op4=2/>
; DUMP-NEXT:    <INST_PHI op0=0 op1=8 op2=1 op3=4 op4=2/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=7/>
; DUMP-NEXT:    <CONSTANTS_BLOCK>
; DUMP-NEXT:      <SETTYPE op0=0/>
; DUMP-NEXT:      <INTEGER op0=8/>
; DUMP-NEXT:    </CONSTANTS_BLOCK>
; DUMP-NEXT:    <INST_ALLOCA op0=1 op1=4/>
; DUMP-NEXT:    <INST_BR op0=1 op1=2 op2=3/>
; DUMP-NEXT:    <INST_BR op0=3 op1=4 op2=3/>
; DUMP-NEXT:    <INST_BR op0=5 op1=6 op2=3/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_STORE op0=4 op1=1 op2=1/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:  <FUNCTION_BLOCK>
; DUMP-NEXT:    <DECLAREBLOCKS op0=6/>
; DUMP-NEXT:    <INST_SWITCH op0=0 op1=1 op2=2 op3=4 op4=1 op5=1
; DUMP-NEXT:                   op6=2 op7=3 op8=1 op9=1 op10=4 op11=3
; DUMP-NEXT:                   op12=1 op13=1 op14=8 op15=4 op16=1 op17=1
; DUMP-NEXT:                   op18=10 op19=4/>
; DUMP-NEXT:    <INST_BR op0=5/>
; DUMP-NEXT:    <INST_BR op0=5/>
; DUMP-NEXT:    <INST_BR op0=5/>
; DUMP-NEXT:    <INST_BR op0=5/>
; DUMP-NEXT:    <INST_RET/>
; DUMP-NEXT:  </FUNCTION_BLOCK>
; DUMP-NEXT:</MODULE_BLOCK>


