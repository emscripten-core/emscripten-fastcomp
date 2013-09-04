; Test how we handle eliding pointers in call instructions.

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
; Define some global functions/variables to be used in testing.


@bytes = internal global [4 x i8] c"abcd"
declare void @foo(i32 %i)
declare i32 @bar(i32* %i)

; ------------------------------------------------------
; Test how we handle a direct call.

define void @DirectCall() {
  call void @foo(i32 0)
  ret void
}

; TD1:      define void @DirectCall() {
; TD1-NEXT:   call void @foo(i32 0)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1:        </CONSTANTS_BLOCK>
; PF1-NEXT:   <INST_CALL op0=0 op1=14 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1-NEXT: </FUNCTION_BLOCK>

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
; Note: This code doesn't follow the PNaCl ABI in that function
; calls can't get pointer arguments. However, intrinsic calls can, and
; this code is a placeholder for such a test.

define void @DirectCallIntToPtrArg(i32 %i) {
  %1 = inttoptr i32 %i to i32*
  %2 = call i32 @bar(i32* %1)
  ret void
}

; TD1:      define void @DirectCallIntToPtrArg(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to i32*
; TD1-NEXT:   %2 = call i32 @bar(i32* %1)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CAST op0=1 op1=4 op2=10/>
; PF1-NEXT:   <INST_CALL op0=0 op1=14 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @DirectCallIntToPtrArg(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to i32*
; TD2-NEXT:   %2 = call i32 @bar(i32* %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=13 op2=1/>
; PF2-NEXT:   <INST_RET/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle a direct call with a normalized ptroint argument.

define void @DirectCallPtrToIntArg() {
  %1 = alloca i8, i32 4, align 8
  %2 = ptrtoint i8* %1 to i32
  call void @foo(i32 %2)
  ret void
}

; TD1:      define void @DirectCallPtrToIntArg() {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   call void @foo(i32 %2)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1:        </CONSTANTS_BLOCK>
; PF1-NEXT:   <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:   <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:   <INST_CALL op0=0 op1=16 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1-NEXT: </FUNCTION_BLOCK>

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
  %1 = bitcast [4 x i8]* @bytes to i32*
  %2 = call i32 @bar(i32* %1)
  ret void
}

; TD1:      define void @DirectCallBitcastArg(i32 %i) {
; TD1-NEXT:   %1 = bitcast [4 x i8]* @bytes to i32*
; TD1-NEXT:   %2 = call i32 @bar(i32* %1)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CAST op0=2 op1=4 op2=11/>
; PF1-NEXT:   <INST_CALL op0=0 op1=14 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @DirectCallBitcastArg(i32 %i) {
; TD2-NEXT:   %1 = bitcast [4 x i8]* @bytes to i32*
; TD2-NEXT:   %2 = call i32 @bar(i32* %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=13 op2=2/>
; PF2-NEXT:   <INST_RET/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle a direct call with a pointer to scalar conversion.

define void @DirectCallScalarArg(i32* %ptr) {
  %1 = ptrtoint [4 x i8]* @bytes to i32
  call void @foo(i32 %1)
  ret void
}

; TD1:      define void @DirectCallScalarArg(i32* %ptr) {
; TD1-NEXT:   %1 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   call void @foo(i32 %1)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CAST op0=2 op1=0 op2=9/>
; PF1-NEXT:   <INST_CALL op0=0 op1=15 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @DirectCallScalarArg(i32* %ptr) {
; TD2-NEXT:   %1 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   call void @foo(i32 %1)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL op0=0 op1=14 op2=2/>
; PF2-NEXT:   <INST_RET/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle an indirect call.

define void @IndirectCall(i32 %i) {
  %1 = inttoptr i32 %i to void (i32)*
  call void %1(i32 %i)
  ret void
}

; TD1:      define void @IndirectCall(i32 %i) {
; TD1-NEXT:   %1 = inttoptr i32 %i to void (i32)*
; TD1-NEXT:   call void %1(i32 %i)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CAST op0=1 op1=3 op2=10/>
; PF1-NEXT:   <INST_CALL op0=0 op1=1 op2=2/>
; PF1-NEXT:   <INST_RET/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @IndirectCall(i32 %i) {
; TD2-NEXT:   %1 = inttoptr i32 %i to void (i32)*
; TD2-NEXT:   call void %1(i32 %i)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL_INDIRECT op0=0 op1=1 op2=2 op3=1/>
; PF2-NEXT:   <INST_RET/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle an indirect call with a normalized ptrtoint argument.

define void @IndirectCallPtrToIntArg(i32 %i) {
  %1 = alloca i8, i32 4, align 8
  %2 = inttoptr i32 %i to void (i32)*
  %3 = ptrtoint i8* %1 to i32
  call void %2(i32 %3)
  ret void
}

; TD1:      define void @IndirectCallPtrToIntArg(i32 %i) {
; TD1-NEXT:   %1 = alloca i8, i32 4, align 8
; TD1-NEXT:   %2 = inttoptr i32 %i to void (i32)*
; TD1-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD1-NEXT:   call void %2(i32 %3)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1:        </CONSTANTS_BLOCK>
; PF1-NEXT:   <INST_ALLOCA op0=1 op1=4/>
; PF1-NEXT:   <INST_CAST op0=3 op1=3 op2=10/>
; PF1-NEXT:   <INST_CAST op0=2 op1=0 op2=9/>
; PF1-NEXT:   <INST_CALL op0=0 op1=2 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @IndirectCallPtrToIntArg(i32 %i) {
; TD2-NEXT:   %1 = alloca i8, i32 4, align 8
; TD2-NEXT:   %2 = inttoptr i32 %i to void (i32)*
; TD2-NEXT:   %3 = ptrtoint i8* %1 to i32
; TD2-NEXT:   call void %2(i32 %3)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2:        </CONSTANTS_BLOCK>
; PF2-NEXT:   <INST_ALLOCA op0=1 op1=4/>
; PF2-NEXT:   <INST_CALL_INDIRECT op0=0 op1=3 op2=2 op3=1/>
; PF2-NEXT:   <INST_RET/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle an indirect call with a pointer to scalar conversion.

define void @IndirectCallScalarArg(i32 %i, i32* %ptr) {
  %1 = inttoptr i32 %i to void (i32)*
  %2 = ptrtoint [4 x i8]* @bytes to i32
  call void %1(i32 %2)
  ret void
}

; TD1:      define void @IndirectCallScalarArg(i32 %i, i32* %ptr) {
; TD1-NEXT:   %1 = inttoptr i32 %i to void (i32)*
; TD1-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD1-NEXT:   call void %1(i32 %2)
; TD1-NEXT:   ret void
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CAST op0=2 op1=3 op2=10/>
; PF1-NEXT:   <INST_CAST op0=4 op1=0 op2=9/>
; PF1-NEXT:   <INST_CALL op0=0 op1=2 op2=1/>
; PF1-NEXT:   <INST_RET/>
; PF1:      </FUNCTION_BLOCK>

; TD2:      define void @IndirectCallScalarArg(i32 %i, i32* %ptr) {
; TD2-NEXT:   %1 = inttoptr i32 %i to void (i32)*
; TD2-NEXT:   %2 = ptrtoint [4 x i8]* @bytes to i32
; TD2-NEXT:   call void %1(i32 %2)
; TD2-NEXT:   ret void
; TD2-NEXT: }

; PF2:      <FUNCTION_BLOCK>
; PF2-NEXT:   <DECLAREBLOCKS op0=1/>
; PF2-NEXT:   <INST_CALL_INDIRECT op0=0 op1=2 op2=2 op3=3/>
; PF2-NEXT:   <INST_RET/>
; PF2:      </FUNCTION_BLOCK>

; ------------------------------------------------------
; Test how we handle intrinsics that can return (inherent) pointers, and
; return statements that expect scalar values.

declare i8* @llvm.nacl.read.tp()

define i32 @ReturnPtrIntrinsic() {
  %1 = call i8* @llvm.nacl.read.tp()
  %2 = ptrtoint i8* %1 to i32
  ret i32 %2
}

; TD1:      define i32 @ReturnPtrIntrinsic() {
; TD1-NEXT:   %1 = call i8* @llvm.nacl.read.tp()
; TD1-NEXT:   %2 = ptrtoint i8* %1 to i32
; TD1-NEXT:   ret i32 %2
; TD1-NEXT: }

; PF1:      <FUNCTION_BLOCK>
; PF1-NEXT:   <DECLAREBLOCKS op0=1/>
; PF1-NEXT:   <INST_CALL op0=0 op1=3/>
; PF1-NEXT:   <INST_CAST op0=1 op1=0 op2=9/>
; PF1-NEXT:   <INST_RET op0=1/>
; PF1-NEXT: </FUNCTION_BLOCK>

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
