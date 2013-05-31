; RUN: pnacl-abicheck < %s | FileCheck %s
; RUN: pnacl-abicheck -pnaclabi-allow-debug-metadata < %s | FileCheck %s --check-prefix=DEBUG


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
