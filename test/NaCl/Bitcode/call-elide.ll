; Test how we handle eliding pointers in call instructions.

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-bcanalyzer -dump-records \
; RUN:              | FileCheck %s -check-prefix=PF2

; RUN: llvm-as < %s | pnacl-freeze -allow-local-symbol-tables \
; RUN:              | pnacl-thaw -allow-local-symbol-tables \
; RUN:              | llvm-dis - | FileCheck %s -check-prefix=TD2

; ------------------------------------------------------
; Define some global functions/variables to be used in testing.


@bytes = internal global [4 x i8] c"abcd"
declare void @foo(i32 %i)
declare i32 @llvm.nacl.setjmp(i8* %i)

; ------------------------------------------------------
; Test how we handle a direct call.

define void @DirectCall() {
  call void @foo(i32 0)
  ret void
}

; TD2:      define void @DirectCall() {
; TD2-NEXT:   call void @foo(i32 0)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2:        </CONSTANTS_BLOCK>
; PF2-NEXT:   <INST_CALL op0=0 op1=14 op2=1/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle a direct call with a normalized inttoptr argument.
; Pointer arguments are only allowed for intrinsic calls.

define void @DirectCallIntToPtrArg(i32 %i) {
  %1 = inttoptr i32 %i to i8*
  %2 = call i32 @llvm.nacl.setjmp(i8* %1)
  ret void
}

; TD2:      define void @DirectCallIntToPtrArg(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i8*
; TD2-NEXT:   %2 = call i32 @llvm.nacl.setjmp(i8* %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=13 op2=1/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle a direct call with a normalized ptroint argument.
; Pointer arguments are only allowed for intrinsic calls.

define void @DirectCallPtrToIntArg() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  call void @foo(i32 %2)
  ret void
}

; TD2:      define void @DirectCallPtrToIntArg() {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   call void @foo(i32 %2)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2:        </CONSTANTS_BLOCK>
; PF2-NEXT:   <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:   <INST_CALL op0=0 op1=15 op2=1/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle a direct call with a normalized bitcast argument.

define void @DirectCallBitcastArg(i32 %i) {
  %1 = bitcast [4 x i8]* @bytes to i8*
  %2 = call i32 @llvm.nacl.setjmp(i8* %1)
  ret void
}

; TD2:      define void @DirectCallBitcastArg(i32 %i) {
; TD2-NEXT:   %1 = bitcast [4 x i8]* @bytes to i8*
; TD2-NEXT:   %2 = call i32 @llvm.nacl.setjmp(i8* %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=13 op2=2/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle a direct call with a pointer to scalar conversion.

define void @DirectCallScalarArg() {
  %1 = ptrtoint [4 x i8]* @bytes to i32
  call void @foo(i32 %1)
  ret void
}

; TD2:      define void @DirectCallScalarArg() {
; TD2-NEXT:   %1 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   call void @foo(i32 %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=13 op2=1/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle an indirect call.

define void @IndirectCall(i32 %i) {
  %1 = inttoptr i32 %i to void (i32)*
  call void %1(i32 %i)
  ret void
}

; TD2:      define void @IndirectCall(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to void (i32)*
; TD2-NEXT:   call void %1(i32 %i)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL_INDIRECT op0=0 op1=1 op2=1 op3=1/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle an indirect call with a normalized ptrtoint argument.

define void @IndirectCallPtrToIntArg(i32 %i) {
  %1 = alloca i8, i32 4, align 8
  %2 = inttoptr i32 %i to void (i32)*
  %3 = ptrtoint i8* %1 to i32
  call void %2(i32 %3)
  ret void
}

; TD2:      define void @IndirectCallPtrToIntArg(i32 %i) {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   %3 = inttoptr i32 %i to void (i32)*
; TD2-NEXT:   call void %3(i32 %2)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2:        </CONSTANTS_BLOCK>
; PF2-NEXT:   <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:   <INST_CALL_INDIRECT op0=0 op1=3 op2=1 op3=1/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle an indirect call with a pointer to scalar conversion.

define void @IndirectCallScalarArg(i32 %i) {
  %1 = inttoptr i32 %i to void (i32)*
  %2 = ptrtoint [4 x i8]* @bytes to i32
  call void %1(i32 %2)
  ret void
}

; TD2:      define void @IndirectCallScalarArg(i32 %i) {
; TD2-NEXT:   %1 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   %2 = inttoptr i32 %i to void (i32)*
; TD2-NEXT:   call void %2(i32 %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL_INDIRECT op0=0 op1=1 op2=1 op3=2/>
; PF2-NEXT:   <INST_RET/>
; PF2-NEXT: </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle intrinsics that can return (inherent) pointers, and
; return statements that expect scalar values.

declare i8* @llvm.nacl.read.tp()

define i32 @ReturnPtrIntrinsic() {
  %1 = call i8* @llvm.nacl.read.tp()
  %2 = ptrtoint i8* %1 to i32
  ret i32 %2
}

; TD2:      define i32 @ReturnPtrIntrinsic() {
; TD2-NEXT:   %1 = call i8* @llvm.nacl.read.tp()
; TD2-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD2-NEXT:   ret i32 %2
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=3/>
; PF2-NEXT:   <INST_RET op0=1/>
; PF2-NEXT: </FUNCTION_BLOCK>
