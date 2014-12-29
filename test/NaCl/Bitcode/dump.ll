; Simple tests on dump flags for pnacl-bcanalyzer.

@bytes = internal global [4 x i8] c"abcd"

@ptr_to_ptr = internal global i32 ptrtoint (i32* @ptr to i32)

@ptr = internal global i32 ptrtoint ([4 x i8]* @bytes to i32)

declare i32 @bar(i32)

define void @AllocCastSimple() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  %3 = bitcast [4 x i8]* @bytes to i32*
  store i32 %2, i32* %3, align 1
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

; -------------------------------------------------

; 1) Test dump of records.
; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer --operands-per-line=4 \
; RUN:                   -dump-records \
; RUN:              | FileCheck %s -check-prefix=DR

; DR: <MODULE_BLOCK>
; DR:   <VERSION op0=1/>
; DR:   <BLOCKINFO_BLOCK/>
; DR:   <TYPE_BLOCK_ID>
; DR:     <NUMENTRY op0=7/>
; DR:     <INTEGER op0=32/>
; DR:     <VOID/>
; DR:     <INTEGER op0=1/>
; DR:     <INTEGER op0=8/>
; DR:     <FUNCTION op0=0 op1=0 op2=0/>
; DR:     <FUNCTION op0=0 op1=1/>
; DR:     <FUNCTION op0=0 op1=1 op2=2/>
; DR:   </TYPE_BLOCK_ID>
; DR:   <FUNCTION op0=4 op1=0 op2=1 op3=0/>
; DR:   <FUNCTION op0=5 op1=0 op2=0 op3=0/>
; DR:   <FUNCTION op0=6 op1=0 op2=0 op3=0/>
; DR:   <GLOBALVAR_BLOCK>
; DR:     <COUNT op0=3/>
; DR:     <VAR op0=0 op1=0/>
; DR:     <DATA op0=97 op1=98 op2=99 op3=100/>
; DR:     <VAR op0=0 op1=0/>
; DR:     <RELOC op0=5/>
; DR:     <VAR op0=0 op1=0/>
; DR:     <RELOC op0=3/>
; DR:   </GLOBALVAR_BLOCK>
; DR:   <VALUE_SYMTAB>
; DR:     <ENTRY op0=1 op1=65 op2=108 op3=108
; DR:            op4=111 op5=99 op6=67 op7=97
; DR:            op8=115 op9=116 op10=83 op11=105
; DR:            op12=109 op13=112 op14=108 op15=101/>
; DR:     <ENTRY op0=2 op1=80 op2=104 op3=105
; DR:            op4=66 op5=97 op6=99 op7=107
; DR:            op8=119 op9=97 op10=114 op11=100
; DR:            op12=82 op13=101 op14=102 op15=115/>
; DR:     <ENTRY op0=0 op1=98 op2=97 op3=114/>
; DR:     <ENTRY op0=5 op1=112 op2=116 op3=114/>
; DR:     <ENTRY op0=3 op1=98 op2=121 op3=116
; DR:            op4=101 op5=115/>
; DR:     <ENTRY op0=4 op1=112 op2=116 op3=114
; DR:            op4=95 op5=116 op6=111 op7=95
; DR:            op8=112 op9=116 op10=114/>
; DR:   </VALUE_SYMTAB>
; DR:   <FUNCTION_BLOCK>
; DR:     <DECLAREBLOCKS op0=1/>
; DR:     <CONSTANTS_BLOCK>
; DR:       <SETTYPE op0=0/>
; DR:       <INTEGER op0=8/>
; DR:     </CONSTANTS_BLOCK>
; DR:     <INST_ALLOCA op0=1 op1=4/>
; DR:     <INST_STORE op0=5 op1=1 op2=1/>
; DR:     <INST_RET/>
; DR:   </FUNCTION_BLOCK>
; DR:   <FUNCTION_BLOCK>
; DR:     <DECLAREBLOCKS op0=4/>
; DR:     <CONSTANTS_BLOCK>
; DR:       <SETTYPE op0=0/>
; DR:       <INTEGER op0=8/>
; DR:     </CONSTANTS_BLOCK>
; DR:     <INST_ALLOCA op0=1 op1=4/>
; DR:     <INST_ALLOCA op0=2 op1=4/>
; DR:     <INST_BR op0=1 op1=2 op2=4/>
; DR:     <INST_LOAD op0=2 op1=0 op2=0/>
; DR:     <INST_BR op0=3/>
; DR:     <INST_LOAD op0=3 op1=0 op2=0/>
; DR:     <INST_BR op0=3/>
; DR:     <INST_PHI op0=0 op1=6 op2=1 op3=6
; DR:               op4=2/>
; DR:     <INST_PHI op0=0 op1=6 op2=1 op3=4
; DR:               op4=2/>
; DR:     <INST_RET/>
; DR:   </FUNCTION_BLOCK>
; DR: </MODULE_BLOCK>

; -------------------------------------------------

; 2) Test dump of records with details.
; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer --operands-per-line=4 \
; RUN:                   --dump-records --dump-details \
; RUN:              | FileCheck %s -check-prefix=DRWD

; DRWD: <MODULE_BLOCK abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=2>
; DRWD:   <VERSION abbrev='UNABBREVIATED' op0=1/>
; DRWD:   <BLOCKINFO_BLOCK abbrev='ENTER_SUBBLOCK' NumWords=24 BlockCodeSize=2>
; DRWD:     <SETBID abbrev='UNABBREVIATED' block='VALUE_SYMTAB'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='FIXED(3)' op1='VBR(8)' op2='ARRAY
; DRWD:                    op3='FIXED(8)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(1)' op1='VBR(8)' op2='ARRAY'
; DRWD:                    op3='FIXED(7)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(1)' op1='VBR(8)' op2='ARRAY'
; DRWD:                    op3='CHAR6'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(2)' op1='VBR(8)' op2='ARRAY'
; DRWD:                    op3='CHAR6'/>
; DRWD:     <SETBID abbrev='UNABBREVIATED' block='CONSTANTS_BLOCK'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(1)' op1='FIXED(3)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(4)' op1='VBR(8)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(4)' op1='LIT(0)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(6)' op1='VBR(8)'/>
; DRWD:     <SETBID abbrev='UNABBREVIATED' block='FUNCTION_BLOCK'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(20)' op1='VBR(6)' op2='VBR(4)'
; DRWD:                    op3='VBR(4)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(2)' op1='VBR(6)' op2='VBR(6)'
; DRWD:                    op3='FIXED(4)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(3)' op1='VBR(6)' op2='FIXED(3)'
; DRWD:                    op3='FIXED(4)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(10)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(10)' op1='VBR(6)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(15)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(43)' op1='VBR(6)' op2='FIXED(3)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(24)' op1='VBR(6)' op2='VBR(6)'
; DRWD:                    op3='VBR(4)'/>
; DRWD:     <SETBID abbrev='UNABBREVIATED' block='GLOBALVAR_BLOCK'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(0)' op1='VBR(6)' op2='FIXED(1)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(1)' op1='VBR(8)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(2)' op1='VBR(8)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(3)' op1='ARRAY' op2='FIXED(8)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(4)' op1='VBR(6)'/>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(4)' op1='VBR(6)' op2='VBR(6)'/>
; DRWD:   </BLOCKINFO_BLOCK abbrev='END_BLOCK'>
; DRWD:   <TYPE_BLOCK_ID abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=3>
; DRWD:     <DEFINE_ABBREV abbrev='DEFINE_ABBREV' op0='LIT(21)' op1='FIXED(1)' op2='ARRAY'
; DRWD:                    op3='FIXED(3)'/>
; DRWD:     <NUMENTRY abbrev='UNABBREVIATED' op0=7/>
; DRWD:     <INTEGER abbrev='UNABBREVIATED' op0=32/>
; DRWD:     <VOID abbrev='UNABBREVIATED'/>
; DRWD:     <INTEGER abbrev='UNABBREVIATED' op0=1/>
; DRWD:     <INTEGER abbrev='UNABBREVIATED' op0=8/>
; DRWD:     <FUNCTION abbrev=4 op0=0 op1=0 op2=0/>
; DRWD:     <FUNCTION abbrev=4 op0=0 op1=1/>
; DRWD:     <FUNCTION abbrev=4 op0=0 op1=1 op2=2/>
; DRWD:   </TYPE_BLOCK_ID abbrev='END_BLOCK'>
; DRWD:   <FUNCTION abbrev='UNABBREVIATED' op0=4 op1=0 op2=1
; DRWD:             op3=0/>
; DRWD:   <FUNCTION abbrev='UNABBREVIATED' op0=5 op1=0 op2=0
; DRWD:             op3=0/>
; DRWD:   <FUNCTION abbrev='UNABBREVIATED' op0=6 op1=0 op2=0
; DRWD:             op3=0/>
; DRWD:   <GLOBALVAR_BLOCK abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=4>
; DRWD:     <COUNT abbrev='UNABBREVIATED' op0=3/>
; DRWD:     <VAR abbrev=4 op0=0 op1=0/>
; DRWD:     <DATA abbrev=7 op0=97 op1=98 op2=99
; DRWD:           op3=100/>
; DRWD:     <VAR abbrev=4 op0=0 op1=0/>
; DRWD:     <RELOC abbrev=8 op0=5/>
; DRWD:     <VAR abbrev=4 op0=0 op1=0/>
; DRWD:     <RELOC abbrev=8 op0=3/>
; DRWD:   </GLOBALVAR_BLOCK abbrev='END_BLOCK'>
; DRWD:   <VALUE_SYMTAB abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=3>
; DRWD:     <ENTRY abbrev=6 op0=1 op1=65 op2=108
; DRWD:            op3=108 op4=111 op5=99 op6=67
; DRWD:            op7=97 op8=115 op9=116 op10=83
; DRWD:            op11=105 op12=109 op13=112 op14=108
; DRWD:            op15=101/>
; DRWD:     <ENTRY abbrev=6 op0=2 op1=80 op2=104
; DRWD:            op3=105 op4=66 op5=97 op6=99
; DRWD:            op7=107 op8=119 op9=97 op10=114
; DRWD:            op11=100 op12=82 op13=101 op14=102
; DRWD:            op15=115/>
; DRWD:     <ENTRY abbrev=6 op0=0 op1=98 op2=97
; DRWD:            op3=114/>
; DRWD:     <ENTRY abbrev=6 op0=5 op1=112 op2=116
; DRWD:            op3=114/>
; DRWD:     <ENTRY abbrev=6 op0=3 op1=98 op2=121
; DRWD:            op3=116 op4=101 op5=115/>
; DRWD:     <ENTRY abbrev=6 op0=4 op1=112 op2=116
; DRWD:            op3=114 op4=95 op5=116 op6=111
; DRWD:            op7=95 op8=112 op9=116 op10=114/>
; DRWD:   </VALUE_SYMTAB abbrev='END_BLOCK'>
; DRWD:   <FUNCTION_BLOCK abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=4>
; DRWD:     <DECLAREBLOCKS abbrev='UNABBREVIATED' op0=1/>
; DRWD:     <CONSTANTS_BLOCK abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=3>
; DRWD:       <SETTYPE abbrev=4 op0=0/>
; DRWD:       <INTEGER abbrev=5 op0=8/>
; DRWD:     </CONSTANTS_BLOCK abbrev='END_BLOCK'>
; DRWD:     <INST_ALLOCA abbrev='UNABBREVIATED' op0=1 op1=4/>
; DRWD:     <INST_STORE abbrev=11 op0=5 op1=1 op2=1/>
; DRWD:     <INST_RET abbrev=7/>
; DRWD:   </FUNCTION_BLOCK abbrev='END_BLOCK'>
; DRWD:   <FUNCTION_BLOCK abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=4>
; DRWD:     <DECLAREBLOCKS abbrev='UNABBREVIATED' op0=4/>
; DRWD:     <CONSTANTS_BLOCK abbrev='ENTER_SUBBLOCK' NumWords={{.*}} BlockCodeSize=3>
; DRWD:       <SETTYPE abbrev=4 op0=0/>
; DRWD:       <INTEGER abbrev=5 op0=8/>
; DRWD:     </CONSTANTS_BLOCK abbrev='END_BLOCK'>
; DRWD:     <INST_ALLOCA abbrev='UNABBREVIATED' op0=1 op1=4/>
; DRWD:     <INST_ALLOCA abbrev='UNABBREVIATED' op0=2 op1=4/>
; DRWD:     <INST_BR abbrev='UNABBREVIATED' op0=1 op1=2 op2=4/>
; DRWD:     <INST_LOAD abbrev=4 op0=2 op1=0 op2=0/>
; DRWD:     <INST_BR abbrev='UNABBREVIATED' op0=3/>
; DRWD:     <INST_LOAD abbrev=4 op0=3 op1=0 op2=0/>
; DRWD:     <INST_BR abbrev='UNABBREVIATED' op0=3/>
; DRWD:     <INST_PHI abbrev='UNABBREVIATED' op0=0 op1=6 op2=1
; DRWD:               op3=6 op4=2/>
; DRWD:     <INST_PHI abbrev='UNABBREVIATED' op0=0 op1=6 op2=1
; DRWD:               op3=4 op4=2/>
; DRWD:     <INST_RET abbrev=7/>
; DRWD:   </FUNCTION_BLOCK abbrev='END_BLOCK'>
; DRWD: </MODULE_BLOCK abbrev='END_BLOCK'>
