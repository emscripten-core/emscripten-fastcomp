; RUN: not pnacl-abicheck < %s | FileCheck %s
; RUN: not pnacl-abicheck -pnaclabi-allow-debug-metadata < %s | FileCheck %s --check-prefix=DEBUG


; Metadata is not part of the PNaCl's stable ABI, so normally the ABI
; checker rejects metadata entirely.  However, for debugging support,
; pre-finalized pexes may contain metadata.  When checking a
; pre-finalized pexe, the ABI checker does not check the types in the
; metadata.

; DEBUG-NOT: Named metadata node llvm.dbg.cu is disallowed
; CHECK: Named metadata node llvm.dbg.cu is disallowed
!llvm.dbg.cu = !{!0}
!0 = metadata !{ half 0.0}

; A debuginfo version must always be specified.
; DEBUG-NOT: ignoring debug info with an invalid version
; CHECK-NOT: ignoring debug info with an invalid version
; DEBUG-NOT: Named metadata node llvm.module.flags is disallowed
; CHECK: Named metadata node llvm.module.flags is disallowed
!llvm.module.flags = !{!1}
!1 = metadata !{i32 1, metadata !"Debug Info Version", i32 2}

; CHECK: Named metadata node madeup is disallowed
; DEBUG: Named metadata node madeup is disallowed
!madeup = !{!2}
!2 = metadata !{ half 1.0}
