; Test that the writer elides an inttoptr of a ptrtoint.

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD2


@bytes = internal global [4 x i8] c"abcd"

define void @inttoptr_of_ptrtoint() {
  ; These two instructions are usually replaced with an equivalent
  ; bitcast, but either sequence is allowed by the PNaCl ABI verifier.
  %1 = ptrtoint [4 x i8]* @bytes to i32
  %2 = inttoptr i32 %1 to i8*
  load i8, i8* %2
  ret void
}

; TD2:      define void @inttoptr_of_ptrtoint() {
; TD2-NEXT:   %1 = bitcast [4 x i8]* @bytes to i8*
; TD2-NEXT:   %2 = load i8, i8* %1
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:       <FUNCTION_BLOCK>
; PF2-NEXT:    <DECLAREBLOCKS op0=1/>
; PF2-NEXT:    <INST_LOAD {{.*}}/>
; PF2-NEXT:    <INST_RET/>
; PF2:       </FUNCTION_BLOCK>
