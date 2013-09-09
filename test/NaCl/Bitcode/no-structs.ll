; Tests that even though global variables can define structured types,
; they types are not put into the bitcode file.

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=1 \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF1

; RUN: llvm-as < %s | pnacl-freeze --pnacl-version=2 \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

declare void @func()

@compound = internal global <{ [4 x i8], i32 }>
    <{ [4 x i8] c"home", i32 ptrtoint (void ()* @func to i32) }>

define void @CheckBitcastGlobal() {
  %1 = bitcast <{ [4 x i8], i32}>* @compound to i32*
  %2 = load i32* %1, align 4
  ret void
}

define void @CheckPtrToIntGlobal() {
  %1 = ptrtoint <{ [4 x i8], i32 }>* @compound to i32
  %2 = add i32 %1, 0
  ret void
}

; Note that neither pnacl-version defines a struct type.

; PF1:      <TYPE_BLOCK_ID>
; PF1-NEXT:   <NUMENTRY op0=5/>
; PF1-NEXT:   <INTEGER op0=32/>
; PF1-NEXT:   <VOID/>
; PF1-NEXT:   <FUNCTION op0=0 op1=1/>
; PF1-NEXT:   <POINTER op0=2 op1=0/>
; PF1-NEXT:   <POINTER op0=0 op1=0/>
; PF1-NEXT: </TYPE_BLOCK_ID>

; PF2:      <TYPE_BLOCK_ID>
; PF2-NEXT:   <NUMENTRY op0=3/>
; PF2-NEXT:   <INTEGER op0=32/>
; PF2-NEXT:   <VOID/>
; PF2-NEXT:   <FUNCTION op0=0 op1=1/>
; PF2-NEXT: </TYPE_BLOCK_ID>
