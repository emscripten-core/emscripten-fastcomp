; RUN: pnacl-abicheck < %s | FileCheck %s

; This test checks that the "addrspace" pointer attribute is rejected
; by the PNaCl ABI verifier.  The only allowed address space value is
; 0 (the default).

@var = addrspace(1) global [4 x i8] c"xxxx"
; CHECK: Variable var has addrspace attribute (disallowed)

define void @func() {
  inttoptr i32 0 to i32 addrspace(2)*
; CHECK: disallowed: bad result type: {{.*}} inttoptr {{.*}} addrspace
  ret void
}

; CHECK-NOT: disallowed
