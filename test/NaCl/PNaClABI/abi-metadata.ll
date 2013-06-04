; RUN: pnacl-abicheck < %s | FileCheck %s
; RUN: pnacl-abicheck -pnaclabi-allow-debug-metadata < %s | FileCheck %s --check-prefix=DEBUG


; Metadata is not part of the PNaCl's stable ABI, so normally the ABI
; checker rejects metadata entirely.  However, for debugging support,
; pre-finalized pexes may contain metadata.  When checking a
; pre-finalized pexe, the ABI checker does not check the types in the
; metadata.

; DEBUG-NOT: Named metadata node llvm.dbg.cu is disallowed
; CHECK: Named metadata node llvm.dbg.cu is disallowed
!llvm.dbg.cu = !{!0}
!0 = metadata !{ half 0.0}

; CHECK: Named metadata node madeup is disallowed
; DEBUG: Named metadata node madeup is disallowed
!madeup = !{!1}
!1 = metadata !{ half 1.0}
