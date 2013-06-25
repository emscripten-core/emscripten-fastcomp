; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw - | llvm-dis - | FileCheck %s

; The "datalayout" field is considered to be implicit in the pexe.  It
; is not stored in the pexe; the reader adds it implicitly.
;
; The most important parts of the datalayout for PNaCl are the pointer
; size and the endianness ("e" for little endian).

; CHECK: target datalayout = "e{{.*}}p:32:32:32{{.*}}"
