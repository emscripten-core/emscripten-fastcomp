; RUN: llc < %s -mcpu=generic -mtriple=i686-unknown-nacl -relocation-model=pic -asm-verbose=false -post-RA-scheduler=false | FileCheck %s

@ptr = external global i32*
@dst = external global i32
@src = external global i32

define void @test0() nounwind {
entry:
    store i32* @dst, i32** @ptr
    %tmp.s = load i32* @src
    store i32 %tmp.s, i32* @dst
    ret void
; CHECK-LABEL:    test0:
; CHECK:	calll	.L0$pb
; CHECK-NEXT: .L0$pb:
; CHECK-NEXT:	popl
; CHECK:	addl	$_GLOBAL_OFFSET_TABLE_+(.-.L0$pb),
; CHECK:	movl	dst@GOT(%eax),
; CHECK:	movl	ptr@GOT(%eax),
; CHECK:	movl	src@GOT(%eax),
}
