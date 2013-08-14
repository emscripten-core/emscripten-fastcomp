; Test how we handle eliding ptrtoint instructions.
; TODO(kschimpf) Expand these tests as further CL's are added for issue 3544.

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=1 \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF1

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=1 | pnacl-thaw \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD1

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=2 \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=2 | pnacl-thaw \
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

; TD1:      define void @AllocCastSimple() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; TD1-NEXT:   store i32 %2, i32* %3, align 1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_CAST op0=4 op1=4 op2=11/>
; PF1-NEXT:     <INST_STORE op0=1 op1=2 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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
; in PNaCl version 2, the casts will be reversed.
define void @AllocCastSimpleReversed() {
  %1 = alloca i8, i32 4, align 8
  %2 = bitcast [4 x i8]* @bytes to i32*
  %3 = ptrtoint i8* %1 to i32
  store i32 %3, i32* %2, align 1
  ret void
}

; TD1:      define void @AllocCastSimpleReversed() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = bitcast [4 x i8]* @bytes to i32*
; TD1-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD1-NEXT:   store i32 %3, i32* %2, align 1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=3 op1=4 op2=11/>
; PF1-NEXT:     <INST_CAST op0=2 op1=0 op2=9/>
; PF1-NEXT:     <INST_STORE op0=2 op1=1 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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

; TD1:      define void @AllocCastDelete() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = alloca i8, i32 4, align 8
; TD1-NEXT:   %4 = ptrtoint i8* %3 to i32
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_ALLOCA op0=3 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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
; single instruction, but will get duplicated after reading back the
; bitcode file, since we insert elided casts immediately before each use.
define void @AllocCastOpt() {
  %1 = alloca i8, i32 4, align 8
  %2 = bitcast [4 x i8]* @bytes to i32*
  %3 = ptrtoint i8* %1 to i32
  store i32 %3, i32* %2, align 1
  store i32 %3, i32* %2, align 1
  ret void
}

; TD1:      define void @AllocCastOpt() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = bitcast [4 x i8]* @bytes to i32*
; TD1-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD1-NEXT:   store i32 %3, i32* %2, align 1
; TD1-NEXT:   store i32 %3, i32* %2, align 1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=3 op1=4 op2=11/>
; PF1-NEXT:     <INST_CAST  op0=2 op1=0 op2=9/>
; PF1-NEXT:     <INST_STORE op0=2 op1=1 op2=1 op3=0/>
; PF1-NEXT:     <INST_STORE op0=2 op1=1 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @AllocCastOpt() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %2, i32* %3, align 1
; TD2-NEXT:   %4 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %5 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   store i32 %4, i32* %5, align 1
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

; TD1:      define void @AllocCastMove(i32) {
; TD1-NEXT:   %2 = alloca i8, i32 4, align 8
; TD1-NEXT:   %3 = bitcast [4 x i8]* @bytes to i32*
; TD1-NEXT:   %4 = ptrtoint i8* %2 to i32
; TD1-NEXT:   %5 = add i32 %0, 1
; TD1-NEXT:   store i32 %4, i32* %3, align 1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF1-NEXT:     <INST_CAST op0=5 op1=4 op2=11/>
; PF1-NEXT:     <INST_CAST op0=2 op1=0 op2=9/>
; PF1-NEXT:     <INST_BINOP op0=6 op1=4 op2=0/>
; PF1-NEXT:     <INST_STORE op0=3 op1=2 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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

; TD1:      define void @StoreGlobal() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   %3 = bitcast i8* %1 to i32*
; TD1-NEXT:   store i32 %2, i32* %3, align 1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=3 op1=0 op2=9/>
; PF1-NEXT:     <INST_CAST op0=2 op1=4 op2=11/>
; PF1-NEXT:     <INST_STORE op0=1 op1=2 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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

; TD1:      define void @StoreGlobalCastsReversed() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = bitcast i8* %1 to i32*
; TD1-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   store i32 %3, i32* %2, align 1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=4 op2=11/>
; PF1-NEXT:     <INST_CAST op0=4 op1=0 op2=9/>
; PF1-NEXT:     <INST_STORE op0=2 op1=1 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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

; TD1:      define i32 @StoreGlobalMovePtr2Int() {
; TD1-NEXT:   %1 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   %2 = alloca i8, i32 4, align 8
; TD1-NEXT:   %3 = bitcast i8* %2 to i32*
; TD1-NEXT:   store i32 %1, i32* %3, align 1
; TD1-NEXT:   ret i32 0
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_CAST op0=3 op1=0 op2=9/>
; PF1-NEXT:     <INST_ALLOCA op0=3 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=4 op2=11/>
; PF1-NEXT:     <INST_STORE op0=1 op1=3 op2=1 op3=0/>
; PF1-NEXT:     <INST_RET op0=4/>
; PF1-NEXT:   </FUNCTION_BLOCK>


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

; TD1:      define void @CastAddAlloca() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = add i32 1, 2
; TD1-NEXT:   %4 = add i32 %2, 2
; TD1-NEXT:   %5 = add i32 1, %2
; TD1-NEXT:   %6 = add i32 %2, %2
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_BINOP op0=5 op1=4 op2=0/>
; PF1-NEXT:     <INST_BINOP op0=2 op1=5 op2=0/>
; PF1-NEXT:     <INST_BINOP op0=7 op1=3 op2=0/>
; PF1-NEXT:     <INST_BINOP op0=4 op1=4 op2=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @CastAddAlloca() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = add i32 1, 2
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = add i32 %3, 2
; TD2-NEXT:   %5 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %6 = add i32 1, %5
; TD2-NEXT:   %7 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %8 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %9 = add i32 %7, %8
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

; TD1:      define void @CastAddGlobal() {
; TD1-NEXT:   %1 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   %2 = add i32 1, 2
; TD1-NEXT:   %3 = add i32 %1, 2
; TD1-NEXT:   %4 = add i32 1, %1
; TD1-NEXT:   %5 = add i32 %1, %1
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_CAST op0=3 op1=0 op2=9/>
; PF1-NEXT:     <INST_BINOP op0=3 op1=2 op2=0/>
; PF1-NEXT:     <INST_BINOP op0=2 op1=3 op2=0/>
; PF1-NEXT:     <INST_BINOP op0=5 op1=3 op2=0/>
; PF1-NEXT:     <INST_BINOP op0=4 op1=4 op2=0/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @CastAddGlobal() {
; TD2-NEXT:   %1 = add i32 1, 2
; TD2-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %3 = add i32 %2, 2
; TD2-NEXT:   %4 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %5 = add i32 1, %4
; TD2-NEXT:   %6 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %7 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %8 = add i32 %6, %7
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

; TD1:      define void @CastBinop() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   %4 = sub i32 %2, %3
; TD1-NEXT:   %5 = mul i32 %2, %3
; TD1-NEXT:   %6 = udiv i32 %2, %3
; TD1-NEXT:   %7 = urem i32 %2, %3
; TD1-NEXT:   %8 = srem i32 %2, %3
; TD1-NEXT:   %9 = shl i32 %2, %3
; TD1-NEXT:   %10 = lshr i32 %2, %3
; TD1-NEXT:   %11 = ashr i32 %2, %3
; TD1-NEXT:   %12 = and i32 %2, %3
; TD1-NEXT:   %13 = or i32 %2, %3
; TD1-NEXT:   %14 = xor i32 %2, %3
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_CAST op0=4 op1=0 op2=9/>
; PF1-NEXT:     <INST_BINOP op0=2 op1=1 op2=1/>
; PF1-NEXT:     <INST_BINOP op0=3 op1=2 op2=2/>
; PF1-NEXT:     <INST_BINOP op0=4 op1=3 op2=3/>
; PF1-NEXT:     <INST_BINOP op0=5 op1=4 op2=5/>
; PF1-NEXT:     <INST_BINOP op0=6 op1=5 op2=6/>
; PF1-NEXT:     <INST_BINOP op0=7 op1=6 op2=7/>
; PF1-NEXT:     <INST_BINOP op0=8 op1=7 op2=8/>
; PF1-NEXT:     <INST_BINOP op0=9 op1=8 op2=9/>
; PF1-NEXT:     <INST_BINOP op0=10 op1=9 op2=10/>
; PF1-NEXT:     <INST_BINOP op0=11 op1=10 op2=11/>
; PF1-NEXT:     <INST_BINOP op0=12 op1=11 op2=12/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @CastBinop() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %4 = sub i32 %2, %3
; TD2-NEXT:   %5 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %6 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %7 = mul i32 %5, %6
; TD2-NEXT:   %8 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %9 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %10 = udiv i32 %8, %9
; TD2-NEXT:   %11 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %12 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %13 = urem i32 %11, %12
; TD2-NEXT:   %14 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %15 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %16 = srem i32 %14, %15
; TD2-NEXT:   %17 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %18 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %19 = shl i32 %17, %18
; TD2-NEXT:   %20 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %21 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %22 = lshr i32 %20, %21
; TD2-NEXT:   %23 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %24 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %25 = ashr i32 %23, %24
; TD2-NEXT:   %26 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %27 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %28 = and i32 %26, %27
; TD2-NEXT:   %29 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %30 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %31 = or i32 %29, %30
; TD2-NEXT:   %32 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %33 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %34 = xor i32 %32, %33
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

; TD1:      define void @TestCasts() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = trunc i32 257 to i8
; TD1-NEXT:   %4 = trunc i32 %2 to i8
; TD1-NEXT:   %5 = zext i32 257 to i64
; TD1-NEXT:   %6 = zext i32 %2 to i64
; TD1-NEXT:   %7 = sext i32 -1 to i64
; TD1-NEXT:   %8 = sext i32 %2 to i64
; TD1-NEXT:   %9 = uitofp i32 1 to float
; TD1-NEXT:   %10 = uitofp i32 %2 to float
; TD1-NEXT:   %11 = sitofp i32 -1 to float
; TD1-NEXT:   %12 = sitofp i32 %2 to float
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_CAST op0=6 op1=1 op2=0/>
; PF1-NEXT:     <INST_CAST op0=2 op1=1 op2=0/>
; PF1-NEXT:     <INST_CAST op0=8 op1=10 op2=1/>
; PF1-NEXT:     <INST_CAST op0=4 op1=10 op2=1/>
; PF1-NEXT:     <INST_CAST op0=9 op1=10 op2=2/>
; PF1-NEXT:     <INST_CAST op0=6 op1=10 op2=2/>
; PF1-NEXT:     <INST_CAST op0=9 op1=11 op2=5/>
; PF1-NEXT:     <INST_CAST op0=8 op1=11 op2=5/>
; PF1-NEXT:     <INST_CAST op0=13 op1=11 op2=6/>
; PF1-NEXT:     <INST_CAST op0=10 op1=11 op2=6/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @TestCasts() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = trunc i32 257 to i8
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = trunc i32 %3 to i8
; TD2-NEXT:   %5 = zext i32 257 to i64
; TD2-NEXT:   %6 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %7 = zext i32 %6 to i64
; TD2-NEXT:   %8 = sext i32 -1 to i64
; TD2-NEXT:   %9 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %10 = sext i32 %9 to i64
; TD2-NEXT:   %11 = uitofp i32 1 to float
; TD2-NEXT:   %12 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %13 = uitofp i32 %12 to float
; TD2-NEXT:   %14 = sitofp i32 -1 to float
; TD2-NEXT:   %15 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %16 = sitofp i32 %15 to float
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:        <FUNCTION_BLOCK>
; PF2:          </CONSTANTS_BLOCK>
; PF2-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF2-NEXT:     <INST_CAST op0=5 op1=1 op2=0/>
; PF2-NEXT:     <INST_CAST op0=2 op1=1 op2=0/>
; PF2-NEXT:     <INST_CAST op0=7 op1=10 op2=1/>
; PF2-NEXT:     <INST_CAST op0=4 op1=10 op2=1/>
; PF2-NEXT:     <INST_CAST op0=8 op1=10 op2=2/>
; PF2-NEXT:     <INST_CAST op0=6 op1=10 op2=2/>
; PF2-NEXT:     <INST_CAST op0=8 op1=11 op2=5/>
; PF2-NEXT:     <INST_CAST op0=8 op1=11 op2=5/>
; PF2-NEXT:     <INST_CAST op0=12 op1=11 op2=6/>
; PF2-NEXT:     <INST_CAST op0=10 op1=11 op2=6/>
; PF2-NEXT:     <INST_RET/>
; PF2-NEXT:   </FUNCTION_BLOCK>

; ------------------------------------------------------

; Show that if a ptrtoint is used in something other than known scalar operations,
; it gets copied to the bitcode file
; TODO(kschimpf): Remove this once all scalar operations have been handled.
define void @TestSavedPtrToInt() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = add i32 %2, 0
  %4 = call i32 @bar(i32 %2)
  ret void
}

; TD1:      define void @TestSavedPtrToInt() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = add i32 %2, 0
; TD1-NEXT:   %4 = call i32 @bar(i32 %2)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_BINOP op0=1 op1=3 op2=0/>
; PF1-NEXT:     <INST_CALL op0=0 op1=22 op2=2/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

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
; PF2-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF2-NEXT:     <INST_BINOP op0=1 op1=3 op2=0/>
; PF2-NEXT:     <INST_CALL op0=0 op1=22 op2=2/>
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

; TD1:      define void @CastIcmp() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   %4 = icmp eq i32 1, 2
; TD1-NEXT:   %5 = icmp eq i32 %2, 2
; TD1-NEXT:   %6 = icmp eq i32 1, %3
; TD1-NEXT:   %7 = icmp eq i32 %2, %3
; TD1-NEXT:   %8 = icmp eq i32 %3, %2
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_CAST op0=6 op1=0 op2=9/>
; PF1-NEXT:     <INST_CMP2 op0=6 op1=5 op2=32/>
; PF1-NEXT:     <INST_CMP2 op0=3 op1=6 op2=32/>
; PF1-NEXT:     <INST_CMP2 op0=8 op1=3 op2=32/>
; PF1-NEXT:     <INST_CMP2 op0=5 op1=4 op2=32/>
; PF1-NEXT:     <INST_CMP2 op0=5 op1=6 op2=32/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @CastIcmp() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = icmp eq i32 1, 2
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = icmp eq i32 %3, 2
; TD2-NEXT:   %5 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %6 = icmp eq i32 1, %5
; TD2-NEXT:   %7 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %8 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %9 = icmp eq i32 %7, %8
; TD2-NEXT:   %10 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %11 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %12 = icmp eq i32 %10, %11
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

; TD1:      define void @CastSelect() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   %3 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   %4 = select i1 true, i32 1, i32 2
; TD1-NEXT:   %5 = select i1 true, i32 %2, i32 2
; TD1-NEXT:   %6 = select i1 true, i32 1, i32 %3
; TD1-NEXT:   %7 = select i1 true, i32 %2, i32 %3
; TD1-NEXT:   %8 = select i1 true, i32 %3, i32 %2
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:        <FUNCTION_BLOCK>
; PF1:          </CONSTANTS_BLOCK>
; PF1-NEXT:     <INST_ALLOCA op0=2 op1=4/>
; PF1-NEXT:     <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:     <INST_CAST op0=7 op1=0 op2=9/>
; PF1-NEXT:     <INST_VSELECT op0=7 op1=6 op2=4/>
; PF1-NEXT:     <INST_VSELECT op0=3 op1=7 op2=5/>
; PF1-NEXT:     <INST_VSELECT op0=9 op1=3 op2=6/>
; PF1-NEXT:     <INST_VSELECT op0=5 op1=4 op2=7/>
; PF1-NEXT:     <INST_VSELECT op0=5 op1=6 op2=8/>
; PF1-NEXT:     <INST_RET/>
; PF1-NEXT:   </FUNCTION_BLOCK>

; TD2:      define void @CastSelect() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = select i1 true, i32 1, i32 2
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %4 = select i1 true, i32 %3, i32 2
; TD2-NEXT:   %5 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %6 = select i1 true, i32 1, i32 %5
; TD2-NEXT:   %7 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %8 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %9 = select i1 true, i32 %7, i32 %8
; TD2-NEXT:   %10 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %11 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %12 = select i1 true, i32 %10, i32 %11
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
