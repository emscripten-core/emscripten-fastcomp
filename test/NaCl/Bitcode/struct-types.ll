; Checks if llvm bitcode defines a struct type before the pointer type,
; even if the struct definintion appears after the pointer type, while
; pnacl bitcode moves the pointer before the struct.
; RUN: llvm-as < %s | llvm-bcanalyzer -dump | FileCheck %s -check-prefix=LLVM
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump | FileCheck %s -check-prefix=PNACL

%typeB = type { i8, %typeA, i32, %typeA }
%typeA = type { i16 }

define %typeB* @foo(%typeB* %a) {
  ret %typeB* %a
}

define %typeB* @bar(%typeB* %b) {
  ret %typeB* %b
}

define i16 @bam(i16 %a) {
  ret i16 %a
}

; Show the ordering llvm uses to order types, which is to expand subtypes
; (including accross pointers) before the type. Expands types for functions
; in order: @foo, @bar, @bam.
; LLVM: <TYPE_BLOCK_ID {{.*}}>
;         i8
; LLVM:   <INTEGER op0=8/>
;         i16
; LLVM:   <INTEGER op0=16/>
;         %typeA = type { i16 }
; LLVM:   <STRUCT_NAME abbrevid=7 op0=116 op1=121 op2=112 op3=101 op4=65/>
; LLVM:   <STRUCT_NAMED abbrevid=8 op0=0 op1=1/>
;         i32
; LLVM:   <INTEGER op0=32/>
;         %typeB = type { i8, %typeA, i32, %typeA }
; LLVM:   <STRUCT_NAME abbrevid=7 op0=116 op1=121 op2=112 op3=101 op4=66/>
; LLVM:   <STRUCT_NAMED abbrevid=8 op0=0 op1=0 op2=2 op3=3 op4=2/>
;         %typeB*
; LLVM:   <POINTER abbrevid=4 op0=4 op1=0/>
;         %typeB* (%typeB*)
; LLVM:   <FUNCTION abbrevid=5 op0=0 op1=5 op2=5/>
;         %typeB* (%typeB*)*
; LLVM:   <POINTER abbrevid=4 op0=6 op1=0/>
;         i16 (i16)
; LLVM:   <FUNCTION abbrevid=5 op0=0 op1=1 op2=1/>
;         i16 (i16)*
; LLVM:   <POINTER abbrevid=4 op0=8 op1=0/>
;         type of instruction "RET"
; LLVM:   <VOID/>
; LLVM: </TYPE_BLOCK_ID>

; Show the ordering pnacl-freeze uses to order types.
; PNACL: <TYPE_BLOCK_ID {{.*}}>
;          %typeB*
; PNACL:   <POINTER abbrevid=4 op0=8 op1=0/>
;          i16
; PNACL:   <INTEGER op0=16/>
;          type of instruction "RET"
; PNACL:   <VOID/>
;          %typeA = type { i16 }
; PNACL:   <STRUCT_NAME abbrevid=7 op0=116 op1=121 op2=112 op3=101 op4=65/>
; PNACL:   <STRUCT_NAMED abbrevid=8 op0=0 op1=1/>
;          %typeB* (%typeB*)
; PNACL:   <FUNCTION abbrevid=5 op0=0 op1=0 op2=0/>
;          %typeB* (%typeB*)*
; PNACL:   <POINTER abbrevid=4 op0=4 op1=0/>
;          i8
; PNACL:   <INTEGER op0=8/>
;          i32
; PNACL:   <INTEGER op0=32/>
;          %typeB = type { i8, %typeA, i32, %typeA }
; PNACL:   <STRUCT_NAME abbrevid=7 op0=116 op1=121 op2=112 op3=101 op4=66/>
; PNACL:   <STRUCT_NAMED abbrevid=8 op0=0 op1=6 op2=3 op3=7 op4=3/>
;          i16 (i16)
; PNACL:   <FUNCTION abbrevid=5 op0=0 op1=1 op2=1/>
;          i16 (i16)*
; PNACL:   <POINTER abbrevid=4 op0=9 op1=0/>
; PNACL: </TYPE_BLOCK_ID>
