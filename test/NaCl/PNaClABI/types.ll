; RUN: pnacl-abicheck < %s | FileCheck %s
; RUN: pnacl-abicheck -pnaclabi-allow-debug-metadata < %s | FileCheck %s --check-prefix=DEBUG
; Test types allowed by PNaCl ABI

; Basic global types

; TODO(mseaborn): Re-enable integer size checking.
; See https://code.google.com/p/nativeclient/issues/detail?id=3360
; C;HECK: Variable i4 has disallowed type: i4
;@i4 = private global i4 0
; C;HECK: Variable i33 has disallowed type: i33
;@i33 = private global i33 0
; C;HECK: Variable i128 has disallowed type: i128
;@i128 = private global i128 0

; CHECK: Variable hlf has disallowed type: half
@hlf = private global half 0.0
; CHECK: Variable fp80 has disallowed type: x86_fp80
@fp80 = private global x86_fp80 undef
; CHECK: Variable f128 has disallowed type: fp128
@f128 = private global fp128 undef
; CHECK: Variable ppc128 has disallowed type: ppc_fp128
@ppc128 = private global ppc_fp128 undef
; CHECK: Variable mmx has disallowed type: x86_mmx
@mmx = private global x86_mmx undef

@i1 = private global i1 0
@i8 = private global i8 0
@i16 = private global i16 0
@i32 = private global i32 0
@i64 = private global i64 0
@flt = private global float 0.0
@dbl = private global double 0.0


; global derived types
@p1 = private global i32* undef
@a1 = private global [1 x i32] undef
@s1 = private global { i32, float } undef
@f1 = private global void (i32) * undef
; CHECK-NOT: disallowed
; CHECK: Variable v1 has disallowed type: <2 x i32>
@v1 = private global <2 x i32> undef
; CHECK-NOT: disallowed
@ps1 = private global <{ i8, i32 }> undef


; named types. with the current implementation, bogus named types are legal
; until they are actually attempted to be used. Might want to fix that.
%struct.s1 = type { half, float}
; CHECK: Variable s11 has disallowed type: %struct.s1 = type { half, float }
@s11 = private global %struct.s1 undef
; CHECK-NOT: disallowed
%struct.s2 = type { i32, i32}
@s12 = private global %struct.s2 undef


; types in arrays, structs, etc
; CHECK: Variable p2 has disallowed type: half*
@p2 = private global half* undef
; TODO(mseaborn): Re-enable integer size checking.
; C;HECK: Variable a2 has disallowed type: [2 x i33]
;@a2 = private global [ 2 x i33 ] undef
; CHECK: Variable s2 has disallowed type: { half, i32 }
@s2 = private global { half, i32 } undef
; C;HECK: Variable s3 has disallowed type: { float, i33 }
;@s3 = private global { float, i33 } undef
; CHECK: Variable s4 has disallowed type: { i32, { i32, half }, float }
@s4 = private global { i32, { i32, half }, float } undef
; CHECK-NOT: disallowed
@s5 = private global { i32, { i32, double }, float } undef

; Initializers with constexprs
; CHECK: Variable cc1 has disallowed type: half
@cc1 = private global half 0.0
; CHECK: Initializer for ce1 has disallowed type: half*
@ce1 = private global i8 * bitcast (half* @cc1 to i8*)
@cc2 = private global { i32, half } undef
; CHECK: Initializer for ce2 has disallowed type: { i32, half }*
@ce2 = private global i32 * getelementptr ({ i32, half } * @cc2, i32 0, i32 0)

define void @func_with_block() {
  br label %some_block
some_block:
  ret void
}

@blockaddr = global i8* blockaddress(@func_with_block, %some_block)
; CHECK: Initializer for blockaddr has disallowed type: i8*

; Circularities:  here to make sure the verifier doesn't crash or assert.

; This oddity is perfectly legal according to the IR and ABI verifiers.
; Might want to fix that. (good luck initializing one of these, though.)
%struct.snake = type { i32, %struct.tail }
%struct.tail = type { %struct.snake, i32 }
@foo = private global %struct.snake undef

%struct.linked = type { i32, %struct.linked * }
@list1 = private global %struct.linked { i32 0, %struct.linked* null }

@list2 = private global i32* bitcast (i32** @list2 to i32*)
; CHECK-NOT: disallowed

; CHECK: Variable alias1 is an alias (disallowed)
@alias1 = alias i32* @i32

; CHECK: Function badReturn has disallowed type: half* ()
declare half* @badReturn()

; CHECK: Function badArgType1 has disallowed type: void (half, i32)
declare void @badArgType1(half %a, i32 %b)
; CHECK: Function badArgType2 has disallowed type: void (i32, half)
declare void @badArgType2(i32 %a, half %b)

; If the metadata is allowed we want to check for types.
; We have a hacky way to test this. The -allow-debug-metadata whitelists debug
; metadata.  That allows us to check types within debug metadata, even though
; debug metadata normally does not have illegal types.
; DEBUG-NOT: Named metadata node llvm.dbg.cu is disallowed
; DEBUG: Named metadata node llvm.dbg.cu refers to disallowed type: half
; CHECK: Named metadata node llvm.dbg.cu is disallowed
!llvm.dbg.cu = !{!0}
!0 = metadata !{ half 0.0}

; CHECK: Named metadata node madeup is disallowed
; DEBUG: Named metadata node madeup is disallowed
!madeup = !{!1}
!1 = metadata !{ half 1.0}
