// RUN: llvm-mc -filetype asm -triple i386-unknown-nacl %s | FileCheck %s

	ret
// CHECK:  	popl	%ecx
// CHECK-NEXT: .bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock
	
	ret $12
// CHECK:  	popl 	%ecx
// CHECK-NEXT:  addl	$12, %esp
// CHECK-NEXT:  .bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock

