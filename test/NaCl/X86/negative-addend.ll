; RUN: pnacl-llc -mtriple=i386-unknown-nacl -filetype=obj %s -o - \
; RUN:   | llvm-objdump -r - | FileCheck %s -check-prefix=X8632
; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=obj %s -o - \
; RUN:   | llvm-objdump -r - | FileCheck %s -check-prefix=X8664

; Check that "add" works for negative values when used as a
; ConstantExpr in a global variable initializer.
; See: https://code.google.com/p/nativeclient/issues/detail?id=3548


; @spacer and @var end up in the BSS section.
; @spacer is at offset 0.  @var is at offset 4096 = 0x1000.

@spacer = internal global [4096 x i8] zeroinitializer
@var = internal global i32 zeroinitializer

@negative_offset = internal global i32 add
    (i32 ptrtoint (i32* @var to i32), i32 -8)

; Note that the addend 4294971384 below equals 0x100000ff8, where
; 0xff8 comes from subtracting 8 from the offset of @var.

; X8632: RELOCATION RECORDS FOR [.data]:
; X8632-NEXT: 0 R_386_32 Unknown

; X8664: RELOCATION RECORDS FOR [.data]:
; X8664-NEXT: 0 R_X86_64_32 .bss+4294971384
