; RUN: llc -filetype=obj -mtriple armv7-none-nacl-gnueabi %s -o - \
; RUN:   | llvm-objdump -triple armv7 -s - | FileCheck %s

; Tests that NaCl object files contain an ELF note section that identifies them
; to the binutils gold linker

define void @main() {
  ret void
}

; There appears to be no way for llvm-objdump to show flags for sections, or
; to dump groups like readelf.
; CHECK: .group
; CHECK: .note.NaCl.ABI.arm
; The contents of the words in the note section should be:
;   sizeof "NaCl"
;   sizeof "arm"
;   1 (NT_VERSION)
;   "NaCl" with nul termination and padding to align 4
;   "arm" with nul termination and padding to align 4
; CHECK-NEXT: 0000 05000000 04000000 01000000 4e61436c
; CHECK-NEXT: 0010 00000000 61726d00
