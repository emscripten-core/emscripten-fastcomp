; Test how we handle eliding ptrtoint instructions.

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-thaw -allow-local-symbol-tables \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD2

; ------------------------------------------------------

declare i32 @bar(i32)

@bytes = internal global [4 x i8] c"abcd"

; ------------------------------------------------------

; Show simple case where we use ptrtoint
define void @AllocCastSimple() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

; TD2:      define void @AllocCastSimple() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_STORE op0=3 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Same as above, but with the cast order changed. Shows
; that we always inject casts back in a fixed order. Hence,
; the casts will be reversed.
define void @AllocCastSimpleReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = bitcast [4 x i8]* @bytes to i32*
  %3 = ptrtoint i8* %1 to i32
  store i32 %3, i32* %2, align 1
  ret void
}

; TD2:      define void @AllocCastSimpleReversed() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_STORE op0=3 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show case where we delete ptrtoint because they aren't used.
define void @AllocCastDelete() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = alloca i8, i32 4, align 8
  %4 = ptrtoint i8* %3 to i32
  ret void
}

; TD2:      define void @AllocCastDelete() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = alloca i8, i32 4, align 8
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show case where we have optimized the ptrtoint (and bitcast) into a
; single instruction, and will only be inserted before the first use
; in the block.
define void @AllocCastOpt() {
  %1 = alloca i8, i32 4, align 8
  %2 = bitcast [4 x i8]* @bytes to i32*
  %3 = ptrtoint i8* %1 to i32
  store i32 %3, i32* %2, align 1
  store i32 %3, i32* %2, align 1
  ret void
}

; TD2:      define void @AllocCastOpt() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_STORE op0=3 op1=1 op2=1/>
; PF2-NEXT:     <INST_STORE op0=3 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show case where ptrtoint (and bitcast) for store are not immediately
; before the store, the casts will be moved to the store.
define void @AllocCastMove(i32) {
  %2 = alloca i8, i32 4, align 8
  %3 = bitcast [4 x i8]* @bytes to i32*
  %4 = ptrtoint i8* %2 to i32
  %5 = add i32 %0, 1
  store i32 %4, i32* %3, align 1
  ret void
}

; TD2:      define void @AllocCastMove(i32) {
; TD2-NEXT:   %2 = alloca i8, i32 4, align 8
; TD2-NEXT:   %3 = add i32 %0, 1
; TD2-NEXT:   %4 = ptrtoint i8* %2 to i32
; TD2-NEXT:   %5 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %4, i32* %5, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_BINOP op0=4 op1=2 op2=0/>
; PF2-NEXT:     <INST_STORE op0=6 op1=2 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show case where ptrtoint on global variable is merged in a store, and
; order is kept.
define void @StoreGlobal() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint [4 x i8]* @bytes to i32
  %3 = bitcast i8* %1 to i32*
  store i32 %2, i32* %3, align 1
  ret void
}

; TD2:      define void @StoreGlobal() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %3 = bitcast i8* %1 to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_STORE op0=1 op1=3 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Same as above, but with cast order reversed.
define void @StoreGlobalCastsReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = bitcast i8* %1 to i32*
  %3 = ptrtoint [4 x i8]* @bytes to i32
  store i32 %3, i32* %2, align 1
  ret void
}

; TD2:      define void @StoreGlobalCastsReversed() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %3 = bitcast i8* %1 to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_STORE op0=1 op1=3 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we will move the ptrtoint of a global to the use.
define i32 @StoreGlobalMovePtr2Int() {
  %1 = ptrtoint [4 x i8]* @bytes to i32
  %2 = alloca i8, i32 4, align 8
  %3 = bitcast i8* %2 to i32*
  store i32 %1, i32* %3, align 1
  ret i32 0
}

; TD2:      define i32 @StoreGlobalMovePtr2Int() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %3 = bitcast i8* %1 to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   ret i32 0
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_STORE op0=1 op1=4 op2=1/>
; PF2-NEXT:     <INST_RET op0=2/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we handle add instructions with pointer casts.
define void @CastAddAlloca() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32

  ; Simple add.
  %3 = add i32 1, 2

  ; Cast first.
  %4 = add i32 %2, 2

  ; Cast second.
  %5 = add i32 1, %2

  ; Cast both.
  %6 = add i32 %2, %2

  ret void
}

; TD2:      define void @CastAddAlloca() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = add i32 1, 2
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = add i32 %3, 2
; TD2-NEXT:   %5 = add i32 1, %3
; TD2-NEXT:   %6 = add i32 %3, %3
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_BINOP op0=4 op1=3 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=2 op1=4 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=6 op1=3 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=4 op1=4 op2=0/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we handle add instructions with pointer casts.
define void @CastAddGlobal() {
  %1 = ptrtoint [4 x i8]* @bytes to i32

  ; Simple Add.
  %2 = add i32 1, 2

  ; Cast first.
  %3 = add i32 %1, 2

  ; Cast Second.
  %4 = add i32 1, %1

  ; Cast both.
  %5 = add i32 %1, %1
  ret void
}

; TD2:      define void @CastAddGlobal() {
; TD2-NEXT:   %1 = add i32 1, 2
; TD2-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %3 = add i32 %2, 2
; TD2-NEXT:   %4 = add i32 1, %2
; TD2-NEXT:   %5 = add i32 %2, %2
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_BINOP op0=2 op1=1 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=4 op1=2 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=4 op1=5 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=6 op1=6 op2=0/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we can handle pointer conversions for other scalar binary operators.
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

; TD2:      define void @CastBinop() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %4 = sub i32 %2, %3
; TD2-NEXT:   %5 = mul i32 %2, %3
; TD2-NEXT:   %6 = udiv i32 %2, %3
; TD2-NEXT:   %7 = urem i32 %2, %3
; TD2-NEXT:   %8 = srem i32 %2, %3
; TD2-NEXT:   %9 = shl i32 %2, %3
; TD2-NEXT:   %10 = lshr i32 %2, %3
; TD2-NEXT:   %11 = ashr i32 %2, %3
; TD2-NEXT:   %12 = and i32 %2, %3
; TD2-NEXT:   %13 = or i32 %2, %3
; TD2-NEXT:   %14 = xor i32 %2, %3
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_BINOP op0=1 op1=3 op2=1/>
; PF2-NEXT:     <INST_BINOP op0=2 op1=4 op2=2/>
; PF2-NEXT:     <INST_BINOP op0=3 op1=5 op2=3/>
; PF2-NEXT:     <INST_BINOP op0=4 op1=6 op2=5/>
; PF2-NEXT:     <INST_BINOP op0=5 op1=7 op2=6/>
; PF2-NEXT:     <INST_BINOP op0=6 op1=8 op2=7/>
; PF2-NEXT:     <INST_BINOP op0=7 op1=9 op2=8/>
; PF2-NEXT:     <INST_BINOP op0=8 op1=10 op2=9/>
; PF2-NEXT:     <INST_BINOP op0=9 op1=11 op2=10/>
; PF2-NEXT:     <INST_BINOP op0=10 op1=12 op2=11/>
; PF2-NEXT:     <INST_BINOP op0=11 op1=13 op2=12/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we handle (non-special) bitcasts by converting pointer
; casts to integer.
define void @TestCasts() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32

  %3 = trunc i32 257 to i8
  %4 = trunc i32 %2 to i8

  %5 = zext i32 257 to i64
  %6 = zext i32 %2 to i64

  %7 = sext i32 -1 to i64
  %8 = sext i32 %2 to i64

  %9 = uitofp i32 1 to float
  %10 = uitofp i32 %2 to float

  %11 = sitofp i32 -1 to float
  %12 = sitofp i32 %2 to float
  ret void
}

; TD2:      define void @TestCasts() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = trunc i32 257 to i8
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = trunc i32 %3 to i8
; TD2-NEXT:   %5 = zext i32 257 to i64
; TD2-NEXT:   %6 = zext i32 %3 to i64
; TD2-NEXT:   %7 = sext i32 -1 to i64
; TD2-NEXT:   %8 = sext i32 %3 to i64
; TD2-NEXT:   %9 = uitofp i32 1 to float
; TD2-NEXT:   %10 = uitofp i32 %3 to float
; TD2-NEXT:   %11 = sitofp i32 -1 to float
; TD2-NEXT:   %12 = sitofp i32 %3 to float
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_CAST op0=5 op1={{.*}} op2=0/>
; PF2-NEXT:     <INST_CAST op0=2 op1={{.*}} op2=0/>
; PF2-NEXT:     <INST_CAST op0=7 op1={{.*}} op2=1/>
; PF2-NEXT:     <INST_CAST op0=4 op1={{.*}} op2=1/>
; PF2-NEXT:     <INST_CAST op0=8 op1={{.*}} op2=2/>
; PF2-NEXT:     <INST_CAST op0=6 op1={{.*}} op2=2/>
; PF2-NEXT:     <INST_CAST op0=8 op1={{.*}} op2=5/>
; PF2-NEXT:     <INST_CAST op0=8 op1={{.*}} op2=5/>
; PF2-NEXT:     <INST_CAST op0=12 op1={{.*}} op2=6/>
; PF2-NEXT:     <INST_CAST op0=10 op1={{.*}} op2=6/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we elide a ptrtoint cast for a call.
define void @TestSavedPtrToInt() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = add i32 %2, 0
  %4 = call i32 @bar(i32 %2)
  ret void
}

; TD2:      define void @TestSavedPtrToInt() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = add i32 %2, 0
; TD2-NEXT:   %4 = call i32 @bar(i32 %2)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_BINOP op0=1 op1=2 op2=0/>
; PF2-NEXT:     <INST_CALL op0=0 op1=25 op2=2/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we can handle pointer conversions for icmp.
define void @CastIcmp() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = ptrtoint [4 x i8]* @bytes to i32
  %4 = icmp eq i32 1, 2
  %5 = icmp eq i32 %2, 2
  %6 = icmp eq i32 1, %3
  %7 = icmp eq i32 %2, %3
  %8 = icmp eq i32 %3, %2
  ret void
}

; TD2:      define void @CastIcmp() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = icmp eq i32 1, 2
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = icmp eq i32 %3, 2
; TD2-NEXT:   %5 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %6 = icmp eq i32 1, %5
; TD2-NEXT:   %7 = icmp eq i32 %3, %5
; TD2-NEXT:   %8 = icmp eq i32 %5, %3
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_CMP2 op0=4 op1=3 op2=32/>
; PF2-NEXT:     <INST_CMP2 op0=2 op1=4 op2=32/>
; PF2-NEXT:     <INST_CMP2 op0=6 op1=7 op2=32/>
; PF2-NEXT:     <INST_CMP2 op0=4 op1=8 op2=32/>
; PF2-NEXT:     <INST_CMP2 op0=9 op1=5 op2=32/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we can handle pointer conversions for Select.
define void @CastSelect() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = ptrtoint [4 x i8]* @bytes to i32
  %4 = select i1 true, i32 1, i32 2
  %5 = select i1 true, i32 %2, i32 2
  %6 = select i1 true, i32 1, i32 %3
  %7 = select i1 true, i32 %2, i32 %3
  %8 = select i1 true, i32 %3, i32 %2
  ret void
}

; TD2:      define void @CastSelect() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = select i1 true, i32 1, i32 2
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = select i1 true, i32 %3, i32 2
; TD2-NEXT:   %5 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %6 = select i1 true, i32 1, i32 %5
; TD2-NEXT:   %7 = select i1 true, i32 %3, i32 %5
; TD2-NEXT:   %8 = select i1 true, i32 %5, i32 %3
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_VSELECT op0=5 op1=4 op2=2/>
; PF2-NEXT:     <INST_VSELECT op0=2 op1=5 op2=3/>
; PF2-NEXT:     <INST_VSELECT op0=7 op1=8 op2=4/>
; PF2-NEXT:     <INST_VSELECT op0=4 op1=9 op2=5/>
; PF2-NEXT:     <INST_VSELECT op0=10 op1=5 op2=6/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that if a phi node refers to a pointer cast, we add
; them at the end of the incoming block.
define void @PhiBackwardRefs(i1) {
  %2 = alloca i8, i32 4, align 8
  %3 = bitcast i8* %2 to i32*
  %4 = alloca i8, i32 4, align 8
  %5 = ptrtoint i8* %4 to i32
  br i1 %0, label %true, label %false

true:
  %6 = load i32* %3
  br label %merge

false:
  %7 = load i32* %3
  br label %merge

merge:
  %8 = phi i32 [%5, %true], [%5, %false]
  %9 = phi i32 [%6, %true], [%7, %false]
  ret void
}

; TD2:      define void @PhiBackwardRefs(i1) {
; TD2-NEXT:   %2 = alloca i8, i32 4, align 8
; TD2-NEXT:   %3 = alloca i8, i32 4, align 8
; TD2-NEXT:   br i1 %0, label %true, label %false
; TD2:      true:
; TD2-NEXT:   %4 = bitcast i8* %2 to i32*
; TD2-NEXT:   %5 = load i32* %4
; TD2-NEXT:   %6 = ptrtoint i8* %3 to i32
; TD2-NEXT:   br label %merge
; TD2:      false:
; TD2-NEXT:   %7 = bitcast i8* %2 to i32*
; TD2-NEXT:   %8 = load i32* %7
; TD2-NEXT:   %9 = ptrtoint i8* %3 to i32
; TD2-NEXT:   br label %merge
; TD2:      merge:
; TD2-NEXT:   %10 = phi i32 [ %6, %true ], [ %9, %false ]
; TD2-NEXT:   %11 = phi i32 [ %5, %true ], [ %8, %false ]
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_BR op0=1 op1=2 op2=4/>
; PF2-NEXT:     <INST_LOAD op0=2 op1=0 op2=0/>
; PF2-NEXT:     <INST_BR op0=3/>
; PF2-NEXT:     <INST_LOAD op0=3 op1=0 op2=0/>
; PF2-NEXT:     <INST_BR op0=3/>
; PF2-NEXT:     <INST_PHI op0=0 op1=6 op2=1 op3=6 op4=2/>
; PF2-NEXT:     <INST_PHI op0=0 op1=6 op2=1 op3=4 op4=2/>
; PF2-NEXT:     <INST_RET/>
; PF2:        </FUNCTION_BLOCK>

; ------------------------------------------------------

; Like PhiBackwardRefs except the phi nodes forward reference
; instructions instead of backwards references.
define void @PhiForwardRefs(i1) {
  br label %start

merge:
  %2 = phi i32 [%9, %true], [%9, %false]
  %3 = phi i32 [%4, %true], [%5, %false]
  ret void

true:
  %4 = load i32* %7
  br label %merge

false:
  %5 = load i32* %7
  br label %merge

start:
  %6 = alloca i8, i32 4, align 8
  %7 = bitcast i8* %6 to i32*
  %8 = alloca i8, i32 4, align 8
  %9 = ptrtoint i8* %8 to i32
  br i1 %0, label %true, label %false
}

; TD2:      define void @PhiForwardRefs(i1) {
; TD2-NEXT:   br label %start
; TD2:      merge
; TD2-NEXT:   %2 = phi i32 [ %11, %true ], [ %11, %false ]
; TD2-NEXT:   %3 = phi i32 [ %5, %true ], [ %7, %false ]
; TD2-NEXT:   ret void
; TD2:      true:
; TD2-NEXT:   %4 = inttoptr i32 %9 to i32*
; TD2-NEXT:   %5 = load i32* %4
; TD2-NEXT:   br label %merge
; TD2:      false:
; TD2-NEXT:   %6 = inttoptr i32 %9 to i32*
; TD2-NEXT:   %7 = load i32* %6
; TD2-NEXT:   br label %merge
; TD2:      start:
; TD2-NEXT:   %8 = alloca i8, i32 4, align 8
; TD2-NEXT:   %9 = ptrtoint i8* %8 to i32
; TD2-NEXT:   %10 = alloca i8, i32 4, align 8
; TD2-NEXT:   %11 = ptrtoint i8* %10 to i32
; TD2-NEXT:   br i1 %0, label %true, label %false
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_BR op0=4/>
; PF2-NEXT:     <FORWARDTYPEREF op0=28 op1=0/>
; PF2-NEXT:     <INST_PHI op0=0 op1=11 op2=2 op3=11 op4=3/>
; PF2-NEXT:     <FORWARDTYPEREF op0=25 op1=0/>
; PF2-NEXT:     <FORWARDTYPEREF op0=26 op1=0/>
; PF2-NEXT:     <INST_PHI op0=0 op1=3 op2=2 op3=5 op4=3/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:     <FORWARDTYPEREF op0=27 op1=0/>
; PF2-NEXT:     <INST_LOAD op0=4294967294 op1=0 op2=0/>
; PF2-NEXT:     <INST_BR op0=1/>
; PF2-NEXT:     <INST_LOAD op0=4294967295 op1=0 op2=0/>
; PF2-NEXT:     <INST_BR op0=1/>
; PF2-NEXT:     <INST_ALLOCA op0=5 op1=4/>
; PF2-NEXT:     <INST_ALLOCA op0=6 op1=4/>
; PF2-NEXT:     <INST_BR op0=2 op1=3 op2=8/>
; PF2:        </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that if a phi node incoming block already has a pointer cast,
; we use it instead of adding one at the end of the block. In this
; example, we reuse instruction %7 in block true for phi node %10.
define void @PhiMergeCast(i1) {
  %2 = alloca i8, i32 4, align 8
  %3 = bitcast i8* %2 to i32*
  %4 = alloca i8, i32 4, align 8
  %5 = ptrtoint i8* %4 to i32
  br i1 %0, label %true, label %false

true:
  %6 = load i32* %3
  %7 = ptrtoint i8* %4 to i32
  %8 = add i32 %6, %7
  br label %merge

false:
  %9 = load i32* %3
  br label %merge

merge:
  %10 = phi i32 [%5, %true], [%5, %false]
  %11 = phi i32 [%6, %true], [%9, %false]
  ret void
}

; TD2:      define void @PhiMergeCast(i1) {
; TD2-NEXT:   %2 = alloca i8, i32 4, align 8
; TD2-NEXT:   %3 = alloca i8, i32 4, align 8
; TD2-NEXT:   br i1 %0, label %true, label %false
; TD2:      true:
; TD2-NEXT:   %4 = bitcast i8* %2 to i32*
; TD2-NEXT:   %5 = load i32* %4
; TD2-NEXT:   %6 = ptrtoint i8* %3 to i32
; TD2-NEXT:   %7 = add i32 %5, %6
; TD2-NEXT:   br label %merge
; TD2:      false:
; TD2-NEXT:   %8 = bitcast i8* %2 to i32*
; TD2-NEXT:   %9 = load i32* %8
; TD2-NEXT:   %10 = ptrtoint i8* %3 to i32
; TD2-NEXT:   br label %merge
; TD2:      merge:
; TD2-NEXT:   %11 = phi i32 [ %6, %true ], [ %10, %false ]
; TD2-NEXT:   %12 = phi i32 [ %5, %true ], [ %9, %false ]
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_BR op0=1 op1=2 op2=4/>
; PF2-NEXT:     <INST_LOAD op0=2 op1=0 op2=0/>
; PF2-NEXT:     <INST_BINOP op0=1 op1=2 op2=0/>
; PF2-NEXT:     <INST_BR op0=3/>
; PF2-NEXT:     <INST_LOAD op0=4 op1=0 op2=0/>
; PF2-NEXT:     <INST_BR op0=3/>
; PF2-NEXT:     <INST_PHI op0=0 op1=8 op2=1 op3=8 op4=2/>
; PF2-NEXT:     <INST_PHI op0=0 op1=8 op2=1 op3=4 op4=2/>
; PF2-NEXT:     <INST_RET/>
; PF2:        </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that we must introduce a cast reference for each
; reachable block, but one is sufficient.
define void @LongReachingCasts(i1) {
  %2 = alloca i8, i32 4, align 8
  %3 = ptrtoint i8* %2 to i32
  %4 = bitcast [4 x i8]* @bytes to i32*
  br i1 %0, label %Split1, label %Split2

Split1:
  br i1 %0, label %b1, label %b2

Split2:
  br i1 %0, label %b3, label %b4

b1:
  store i32 %3, i32* %4, align 1
  store i32 %3, i32* %4, align 1
  ret void

b2:
  store i32 %3, i32* %4, align 1
  store i32 %3, i32* %4, align 1
  ret void

b3:
  store i32 %3, i32* %4, align 1
  store i32 %3, i32* %4, align 1
  ret void

b4:
  store i32 %3, i32* %4, align 1
  store i32 %3, i32* %4, align 1
  ret void
}

; TD2:      define void @LongReachingCasts(i1) {
; TD2-NEXT:   %2 = alloca i8, i32 4, align 8
; TD2-NEXT:   br i1 %0, label %Split1, label %Split2
; TD2:      Split1:
; TD2-NEXT:   br i1 %0, label %b1, label %b2
; TD2:      Split2:
; TD2-NEXT:   br i1 %0, label %b3, label %b4
; TD2:      b1:
; TD2-NEXT:   %3 = ptrtoint i8* %2 to i32
; TD2-NEXT:   %4 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %3, i32* %4, align 1
; TD2-NEXT:   store i32 %3, i32* %4, align 1
; TD2-NEXT:   ret void
; TD2:      b2:
; TD2-NEXT:   %5 = ptrtoint i8* %2 to i32
; TD2-NEXT:   %6 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %5, i32* %6, align 1
; TD2-NEXT:   store i32 %5, i32* %6, align 1
; TD2-NEXT:   ret void
; TD2:      b3:
; TD2-NEXT:   %7 = ptrtoint i8* %2 to i32
; TD2-NEXT:   %8 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %7, i32* %8, align 1
; TD2-NEXT:   store i32 %7, i32* %8, align 1
; TD2-NEXT:   ret void
; TD2:      b4:
; TD2-NEXT:   %9 = ptrtoint i8* %2 to i32
; TD2-NEXT:   %10 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %9, i32* %10, align 1
; TD2-NEXT:   store i32 %9, i32* %10, align 1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:     <INST_BR op0=1 op1=2 op2=3/>
; PF2-NEXT:     <INST_BR op0=3 op1=4 op2=3/>
; PF2-NEXT:     <INST_BR op0=5 op1=6 op2=3/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_STORE op0=4 op1=1 op2=1/>
; PF2-NEXT:     <INST_RET/>
; PF2:        </FUNCTION_BLOCK>
