; RUN: llc -filetype=obj -mtriple i686-none-nacl %s -o - \
; RUN:   | llvm-objdump -triple i686 -s - | FileCheck --check-prefix=I386 %s

; RUN: llc -filetype=obj -mtriple x86_64-none-nacl %s -o - \
; RUN:   | llvm-objdump -triple x86_64 -s - | FileCheck --check-prefix=X8664 %s

; Tests that NaCl object files contain an ELF note section that identifies them
; to the binutils gold linker

define void @main() {
  ret void
}

; There appears to be no way for llvm-objdump to show flags for sections, or
; to dump groups like readelf.
; I386: .group
; I386: .note.NaCl.ABI.x86-32
; The contents of the words in the note section should be:
;   sizeof "NaCl"
;   sizeof "x86-32"
;   1 (NT_VERSION)
;   "NaCl" with nul termination and padding to align 4
;   "x86-32" with nul termination and padding to align 4
; I386-NEXT: 0000 05000000 07000000 01000000 4e61436c
; I386-NEXT: 0010 00000000 7838362d 33320000

; X8664: .group
; X8664: .note.NaCl.ABI.x86-64
; The contents of the words in the note section should be:
;   sizeof "NaCl"
;   sizeof "x86-64"
;   1 (NT_VERSION)
;   "NaCl" with nul termination and padding to align 4
;   q"x86-64" with nul termination and padding to align 4
; X8664-NEXT: 0000 05000000 07000000 01000000 4e61436c
; X8664-NEXT: 0010 00000000 7838362d 36340000
