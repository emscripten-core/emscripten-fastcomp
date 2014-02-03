; Tests that check how we handle supported and unsupported versions of
; the same input. Includes code that tests that we loose names for
; pointer casts in local value symbol tables.

define i8 @foo(i32 %i) {
  %v1 = add i32 %i, %i
  %v2 = inttoptr i32 %v1 to i8*
  %v3 = load i8* %v2
  ret i8 %v3
}

; Test source effects of running with only supported bitcode features.
; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - \
; RUN:              | FileCheck %s -check-prefix=CSUP


; CSUP:      define i8 @foo(i32) {
; CSUP-NEXT:   %2 = add i32 %0, %0
; CSUP-NEXT:   %3 = inttoptr i32 %2 to i8*
; CSUP-NEXT:   %4 = load i8* %3
; CSUP-NEXT:   ret i8 %4
; CSUP-NEXT: }

; Test source effects of running with unsupported bitcode features.
; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-thaw -allow-local-symbol-tables \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=CUNS

; CUNS:      define i8 @foo(i32 %i) {
; CUNS-NEXT:   %v1 = add i32 %i, %i
; CUNS-NEXT:   %1 = inttoptr i32 %v1 to i8*
; CUNS-NEXT:   %v3 = load i8* %1
; CUNS-NEXT:   ret i8 %v3
; CUNS-NEXT: }

; Test dump effects of running with only supported bitcode features.
; RUN: llvm-as < %s | pnacl-freeze | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=DSUP

; DSUP:      <MODULE_BLOCK>
; DSUP-NEXT:   <VERSION op0=1/>
; DSUP-NEXT:   <BLOCKINFO_BLOCK/>
; DSUP-NEXT:   <TYPE_BLOCK_ID>
; DSUP-NEXT:     <NUMENTRY op0=4/>
; DSUP-NEXT:     <INTEGER op0=32/>
; DSUP-NEXT:     <INTEGER op0=8/>
; DSUP-NEXT:     <FUNCTION op0=0 op1=1 op2=0/>
; DSUP-NEXT:     <VOID/>
; DSUP-NEXT:   </TYPE_BLOCK_ID>
; DSUP-NEXT:   <FUNCTION op0=2 op1=0 op2=0 op3=0/>
; DSUP-NEXT:   <GLOBALVAR_BLOCK>
; DSUP-NEXT:     <COUNT op0=0/>
; DSUP-NEXT:   </GLOBALVAR_BLOCK>
; DSUP-NEXT:   <VALUE_SYMTAB>
; DSUP-NEXT:     <ENTRY op0=0 op1=102 op2=111 op3=111/>
; DSUP-NEXT:   </VALUE_SYMTAB>
; DSUP-NEXT:   <FUNCTION_BLOCK>
; DSUP-NEXT:     <DECLAREBLOCKS op0=1/>
; DSUP-NEXT:     <INST_BINOP op0=1 op1=1 op2=0/>
; DSUP-NEXT:     <INST_LOAD op0=1 op1=0 op2=1/>
; DSUP-NEXT:     <INST_RET op0=1/>
; DSUP-NEXT:   </FUNCTION_BLOCK>
; DSUP-NEXT: </MODULE_BLOCK>

; Test dump effects of running with unsupported bitcode features.
; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=DUNS

; DUNS:      <MODULE_BLOCK>
; DUNS-NEXT:   <VERSION op0=1/>
; DUNS-NEXT:   <BLOCKINFO_BLOCK/>
; DUNS-NEXT:   <TYPE_BLOCK_ID>
; DUNS-NEXT:     <NUMENTRY op0=4/>
; DUNS-NEXT:     <INTEGER op0=32/>
; DUNS-NEXT:     <INTEGER op0=8/>
; DUNS-NEXT:     <FUNCTION op0=0 op1=1 op2=0/>
; DUNS-NEXT:     <VOID/>
; DUNS-NEXT:   </TYPE_BLOCK_ID>
; DUNS-NEXT:   <FUNCTION op0=2 op1=0 op2=0 op3=0/>
; DUNS-NEXT:   <GLOBALVAR_BLOCK>
; DUNS-NEXT:     <COUNT op0=0/>
; DUNS-NEXT:   </GLOBALVAR_BLOCK>
; DUNS-NEXT:   <VALUE_SYMTAB>
; DUNS-NEXT:     <ENTRY op0=0 op1=102 op2=111 op3=111/>
; DUNS-NEXT:   </VALUE_SYMTAB>
; DUNS-NEXT:   <FUNCTION_BLOCK>
; DUNS-NEXT:     <DECLAREBLOCKS op0=1/>
; DUNS-NEXT:     <INST_BINOP op0=1 op1=1 op2=0/>
; DUNS-NEXT:     <INST_LOAD op0=1 op1=0 op2=1/>
; DUNS-NEXT:     <INST_RET op0=1/>
; DUNS-NEXT:     <VALUE_SYMTAB>
; DUNS-NEXT:       <ENTRY op0=2 op1=118 op2=49/>
; DUNS-NEXT:       <ENTRY op0=1 op1=105/>
; DUNS-NEXT:       <ENTRY op0=3 op1=118 op2=51/>
; DUNS-NEXT:     </VALUE_SYMTAB>
; DUNS-NEXT:   </FUNCTION_BLOCK>
; DUNS-NEXT: </MODULE_BLOCK>
