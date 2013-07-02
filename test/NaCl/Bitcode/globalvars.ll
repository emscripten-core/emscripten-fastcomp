; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - | FileCheck %s
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump \
; RUN:              | FileCheck %s -check-prefix=BC

; Test that we generate appropriate bitcode values for global variables.

; Make sure that the function declaration for function func (below)
; appears before the global variables block.
; BC: <FUNCTION op0=5 op1=0 op2=0 op3=0/>

; Make sure we begin the globals block after function declarations.
; BC-NEXT: <GLOBALVAR_BLOCK
; BC-NEXT: <COUNT op0=15/>

@bytes = internal global [7 x i8] c"abcdefg"
; CHECK: @bytes = internal global [7 x i8] c"abcdefg"
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <DATA abbrevid=7 op0=97 op1=98 op2=99 op3=100 op4=101 op5=102 op6=103/>


@ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)
; CHECK: @ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=8 op0=5/>

@ptr_to_func = internal global i32 ptrtoint (void ()* @func to i32)
; CHECK: @ptr_to_func = internal global i32 ptrtoint (void ()* @func to i32)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=8 op0=0/>

@compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
; CHECK: @compound = internal global <{ [3 x i8], i32 }> <{ [3 x i8] c"foo", i32 ptrtoint (void ()* @func to i32) }>
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <COMPOUND abbrevid=5 op0=2/>
; BC-NEXT: <DATA abbrevid=7 op0=102 op1=111 op2=111/>
; BC-NEXT: <RELOC abbrevid=8 op0=0/>

@ptr = internal global i32 ptrtoint ([7 x i8]* @bytes to i32)
; CHECK: @ptr = internal global i32 ptrtoint ([7 x i8]* @bytes to i32)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=8 op0=1/>

@addend_ptr = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)
; CHECK: @addend_ptr = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 1)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=5 op1=1/>

@addend_negative = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 -1)
; CHECK: @addend_negative = internal global i32 add (i32 ptrtoint (i32* @ptr to i32), i32 -1)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=5 op1=4294967295/>

@addend_array1 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 1)
; CHECK: @addend_array1 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 1)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=1 op1=1/>

@addend_array2 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 7)
; CHECK: @addend_array2 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 7)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=1 op1=7/>

@addend_array3 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 9)
; CHECK: @addend_array3 = internal global i32 add (i32 ptrtoint ([7 x i8]* @bytes to i32), i32 9)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=1 op1=9/>

@addend_struct1 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)
; CHECK: @addend_struct1 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 1)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=4 op1=1/>

@addend_struct2 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)
; CHECK: @addend_struct2 = internal global i32 add (i32 ptrtoint (<{ [3 x i8], i32 }>* @compound to i32), i32 4)
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=0/>
; BC-NEXT: <RELOC abbrevid=9 op0=4 op1=4/>

@ptr_to_func_align = internal global i32 ptrtoint (void ()* @func to i32), align 8
; CHECK: @ptr_to_func_align = internal global i32 ptrtoint (void ()* @func to i32), align 8
; BC-NEXT: <VAR abbrevid=4 op0=4 op1=0/>
; BC-NEXT: <RELOC abbrevid=8 op0=0/>

@char = internal constant [1 x i8] c"0"
; CHECK: @char = internal constant [1 x i8] c"0"
; BC-NEXT: <VAR abbrevid=4 op0=0 op1=1/>
; BC-NEXT: <DATA abbrevid=7 op0=48/>

@short = internal constant [2 x i8] zeroinitializer
; CHECK: @short = internal constant [2 x i8] zeroinitializer
; BC-NEXT:  <VAR abbrevid=4 op0=0 op1=1/>
; BC-NEXT:  <ZEROFILL abbrevid=6 op0=2/>

; BC-NEXT: </GLOBALVAR_BLOCK>

define void @func() {
  ret void
}

