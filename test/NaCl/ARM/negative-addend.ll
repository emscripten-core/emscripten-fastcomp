; RUN: pnacl-llc -mtriple=arm-unknown-nacl -filetype=obj %s -o - \
; RUN:   | llvm-objdump -r - | FileCheck %s -check-prefix=ARM

; Check that "add" works for negative values when used as a
; ConstantExpr in a global variable initializer.
; See: https://code.google.com/p/nativeclient/issues/detail?id=3548


; @spacer and @var end up in the BSS section.
; @spacer is at offset 0.  @var is at offset 4096 = 0x1000.

@spacer = internal global [4096 x i8] zeroinitializer
@var = internal global i32 zeroinitializer

@negative_offset = internal global i32 add
    (i32 ptrtoint (i32* @var to i32), i32 -8)

; ARM: RELOCATION RECORDS FOR [.data.rel.local]:
; ARM-NEXT: 0 R_ARM_ABS32 .bss
