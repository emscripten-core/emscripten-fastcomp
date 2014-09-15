# RUN: llvm-mc -filetype=obj -triple i686-unknown-nacl %s -o - | \
# RUN:   llvm-objdump -triple i686 -d - | FileCheck %s

    .text
    .globl test_lock_prefix
    .type test_lock_prefix,@function
    .p2align 5
test_lock_prefix:
    .fill 29, 1, 0x90
    lock cmpxchg8b 0x0(%ebp)

# CHECK-LABEL: test_lock_prefix
# CHECK:       1c:  90          nop
# CHECK-NEXT:  1d:  0f 1f 00    nopl (%eax)
# CHECK-NEXT:  20:  f0          lock
# CHECK-NEXT:  21:  0f c7 4d 00 cmpxchg8b (%ebp)

    .globl  test_rep_prefix
    .type   test_rep_prefix,@function
    .p2align 5
test_rep_prefix:
    mov 100, %ecx
    .fill 24, 1, 0x90
    rep movsw

# CHECK-LABEL: test_rep_prefix
# CHECK:       5d:  90    nop
# CHECK-NEXT:  5e:  66 90 nop
# CHECK-NEXT:  60:  f3    rep
# CHECK-NEXT:  61:  66 a5 movsw

    .globl  test_repne_prefix
    .type   test_repne_prefix,@function
    .p2align 5
test_repne_prefix:
    mov 100, %ecx
    .fill 24, 1, 0x90
    repne scasw

# CHECK-LABEL: test_repne_prefix
# CHECK:       9d:  90    nop
# CHECK-NEXT:  9e:  66 90 nop
# CHECK-NEXT:  a0:  f2    repne
# CHECK-NEXT:  a1:  66 af scasw
