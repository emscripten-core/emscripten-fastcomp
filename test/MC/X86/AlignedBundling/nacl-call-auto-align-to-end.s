# RUN: llvm-mc -filetype=obj -triple i686-unknown-nacl %s -o - \
# RUN:   | llvm-objdump -disassemble -no-show-raw-insn - | FileCheck %s

# RUN: llvm-mc -filetype=obj -triple x86_64-unknown-nacl \
# RUN: -sfi-hide-sandbox-base=false %s -o - \
# RUN:   | llvm-objdump -disassemble -no-show-raw-insn - | FileCheck %s

# RUN: llvm-mc -filetype=obj -triple x86_64-unknown-nacl \
# RUN: -sfi-hide-sandbox-base=true %s -o - \
# RUN:   | llvm-objdump -disassemble -no-show-raw-insn - \
# RUN:   | FileCheck --check-prefix=HIDE %s

# Test that bare call instructions in NaCl are automatically aligned to the end
# of a bundle without the need for .bundle_align directives, to match gas's
# behavior.
  .text
foo:
# Each of these mov instructions is 4 bytes long.
  movsd %xmm1,%xmm2
  movsd %xmm1,%xmm2
  movsd %xmm1,%xmm2
  movsd %xmm1,%xmm2
# Each of these movs is 5 bytes long.
  movl $1, %eax
  movl $1, %eax
  call   bar
# To align this call to a bundle end, we need a 1-byte NOP.
# CHECK:        1a:  nop
# CHECK-NEXT:   1b: call

  movsd %xmm1,%xmm2
  movsd %xmm1,%xmm2
  movsd %xmm1,%xmm2
  movsd %xmm1,%xmm2
  movl $1, %eax
  movl $1, %eax
  movl $1, %eax
  call   bar
# Here we have to pad until the end of the *next* boundary because
# otherwise the call crosses a boundary.
# The last byte of the bundle has to be a 1-byte nop so it doesn't
# cross the boundary itself.
# CHECK:      3f: nop
# The remaining nops can be implemented any way the compiler wants.
# CHECK:      5b: call

# HIDE-NOT: call
