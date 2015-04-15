; RUN: llc -filetype=obj -mtriple mipsel-none-nacl %s -o - \
; RUN:   | llvm-objdump -triple mipsel -s - | FileCheck %s

; Tests that NaCl object files contain an ELF note section that identifies them
; to the binutils gold linker

define void @main() {
  ret void
}

; There appears to be no way for llvm-objdump to show flags for sections, or
; to dump groups like readelf.
; CHECK: .group
; CHECK: .note.NaCl.ABI.mipsel
; The contents of the words in the note section should be:
;   sizeof "NaCl"
;   sizeof "mipsel"
;   1 (NT_VERSION)
;   "NaCl" with nul termination and padding to align 4
;   "mipsel" with nul termination and padding to align 4
; CHECK-NEXT: 0000 05000000 07000000 01000000 4e61436c
; CHECK-NEXT: 0010 00000000 6d697073 656c0000
