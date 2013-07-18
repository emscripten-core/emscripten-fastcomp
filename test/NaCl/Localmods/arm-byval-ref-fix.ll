; RUN: pnacl-llc -march=arm -mtriple=armv7a-none-nacl %s -o - | FileCheck %s

; byval is currently crashing on ARM for upstream LLVM (PR11018).
; We have a LOCALMOD in ARMISelLowering to simply leave byval wholly
; on the stack, so this is expected to pass.

%struct.S = type { i32, i32 }

define void @foo(%struct.S* byval %w) nounwind {
entry:

; Verify that 55 is stored onto the stack directly, so the struct is
; passed by value and not by reference.

; CHECK: foo:
; CHECK-NEXT: entry
; CHECK-NEXT: mov [[REG:r[0-9]+]], #55
; CHECK-NEXT: str [[REG]], [sp]

  %x = getelementptr inbounds %struct.S* %w, i32 0, i32 0
  store i32 55, i32* %x, align 4
  ret void
}

define i32 @main() nounwind {
entry:
  %w = alloca %struct.S, align 4
  store %struct.S { i32 0, i32 0 }, %struct.S* %w
  call void @foo(%struct.S* byval %w)
  %x = getelementptr inbounds %struct.S* %w, i32 0, i32 0
  %retval = load i32* %x, align 4
  ret i32 %retval
}
