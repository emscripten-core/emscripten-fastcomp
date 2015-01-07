; RUN: pnacl-llc -mtriple=i686-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck %s --check-prefix=NACL32
; RUN: pnacl-llc -mtriple=i686-unknown-nacl -filetype=asm -O0 %s -o - \
; RUN:   | FileCheck %s --check-prefix=NACL32O0
; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck %s --check-prefix=NACL64
; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm -O0 %s -o - \
; RUN:   | FileCheck %s --check-prefix=NACL64

;;;;
; Call to a NaCl trampoline (specific addresses).
define i32 @call_address() {
entry:
  call void inttoptr (i32 66496 to void ()*)()
  ret i32 0
}
; NACL32-LABEL: call_address
; NACL32: calll 66496
; NACL32O0-LABEL: call_address
; NACL32O0: movl $66496, [[REG:%[a-z0-9]+]]
; NACL32O0: naclcall {{.*}}[[REG]]
; NACL64-LABEL: call_address
; NACL64: movl $66496, [[REG:%[a-z0-9]+]]
; NACL64: naclcall {{.*}}[[REG]],%r15

define fastcc i32 @tail_call_address(i32 %arg) {
entry:
  %call1 = tail call fastcc i32 inttoptr (i32 66496 to i32 (i32)*)(i32 %arg)
  ret i32 %call1
}
; NACL32-LABEL: tail_call_address
; NACL32: movl $66496, [[REG:%[a-z0-9]+]]
; NACL32: nacljmp {{.*}}[[REG]]
; NACL32O0-LABEL: tail_call_address
; NACL32O0: movl $66496, [[REG:%[a-z0-9]+]]
; NACL32O0: nacljmp {{.*}}[[REG]]
; NACL64-LABEL: tail_call_address
; NACL64: movl $66496, [[REG:%[a-z0-9]+]]
; NACL64: nacljmp {{.*}}[[REG]], %r15


;;;;;
; Call to another function (external/internal), directly.

declare void @other_function()

define internal void @call_other_function() {
  call void @other_function()
  ret void
}

define void @call_other_function2() {
  call void @call_other_function()
  ret void
}
; NACL32-LABEL: call_other_function
; NACL32: calll other_function
; NACL32-LABEL: call_other_function2
; NACL32: calll call_other_function
; NACL32O0-LABEL: call_other_function
; NACL32O0: calll other_function
; NACL64-LABEL: call_other_function
; NACL64: call other_function
; NACL64-LABEL: call_other_function2
; NACL64: call call_other_function


declare fastcc i32 @other_function_fast()

define internal fastcc i32 @tail_call_other_function() {
  %i = tail call fastcc i32 @other_function_fast()
  ret i32 %i
}

define fastcc i32 @tail_call_other_function2() {
  %i = tail call fastcc i32 @tail_call_other_function()
  ret i32 %i
}
; NACL32-LABEL: tail_call_other_function
; NACL32: jmp other_function_fast
; NACL32-LABEL: tail_call_other_function2
; NACL32: jmp tail_call_other_function
; NACL32O0-LABEL: tail_call_other_function
; NACL32O0: jmp other_function_fast
; NACL32O0-LABEL: tail_call_other_function2
; NACL32O0: jmp tail_call_other_function
; NACL64-LABEL: tail_call_other_function
; NACL64: jmp other_function_fast
; NACL64-LABEL: tail_call_other_function2
; NACL64: jmp tail_call_other_function

;;;;;
; Indirect call, but not a specific address.

@fp = external global i32 (i32)*

; With a load.
define i32 @call_indirect() {
  %1 = load i32 (i32)** @fp, align 4
  %call1 = call i32 %1(i32 10)
  ret i32 %call1
}
; NACL32-LABEL: call_indirect
; NACL32: movl fp, [[REG:%[a-z0-9]+]]
; NACL32: naclcall {{.*}}[[REG]]
; NACL32O0-LABEL: call_indirect
; NACL32O0: movl fp, [[REG:%[a-z0-9]+]]
; NACL32O0: naclcall {{.*}}[[REG]]
; NACL64-LABEL: call_indirect
; NACL64: movl fp({{.*}}), [[REG:%[a-z0-9]+]]
; NACL64: naclcall {{.*}}[[REG]],%r15

define fastcc i32 @tail_call_indirect() {
  %1 = load i32 (i32)** @fp, align 4
  %call1 = tail call fastcc i32 %1(i32 10)
  ret i32 %call1
}
; NACL32-LABEL: tail_call_indirect
; NACL32: movl fp, [[REG:%[a-z0-9]+]]
; NACL32: nacljmp {{.*}}[[REG]]
; NACL32O0-LABEL: tail_call_indirect
; NACL32O0: movl fp, [[REG:%[a-z0-9]+]]
; NACL32O0: nacljmp {{.*}}[[REG]]
; NACL64-LABEL: tail_call_indirect
; NACL64: movl fp({{.*}}), [[REG:%[a-z0-9]+]]
; NACL64: nacljmp {{.*}}[[REG]], %r15

; "Without" a load (may load from stack on x86-32 still).
define i32 @call_indirect_arg(i32 ()* %argfp) {
  %call1 = call i32 %argfp()
  ret i32 %call1
}
; NACL32-LABEL: call_indirect_arg
; NACL32: naclcall {{%[a-z0-9]+}}
; NACL32O0-LABEL: call_indirect_arg
; NACL32O0: naclcall {{%[a-z0-9]+}}
; NACL64-LABEL: call_indirect_arg
; NACL64: naclcall {{%[a-z0-9]+}},%r15

define fastcc i32 @tail_call_indirect_arg(i32 ()* %argfp) {
  %call1 = tail call fastcc i32 %argfp()
  ret i32 %call1
}
; NACL32-LABEL: tail_call_indirect_arg
; NACL32: nacljmp {{%[a-z0-9]+}}
; NACL32O0-LABEL: tail_call_indirect_arg
; NACL32O0: nacljmp {{%[a-z0-9]+}}
; NACL64-LABEL: tail_call_indirect_arg
; NACL64: nacljmp {{%[a-z0-9]+}}, %r15
