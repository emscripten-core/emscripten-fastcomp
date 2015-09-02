// RUN: llvm-mc -filetype asm -triple i386-unknown-nacl %s | FileCheck %s
.scratch %ecx

// Tests if the jmp instruction is expanded correctly
	
	jmp .L1
// CHECK:	jmp .L1
.L1:

	jmp *%eax
// CHECK: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %eax
// CHECK-NEXT:  jmpl	*%eax
// CHECK-NEXT:  .bundle_unlock

	jmp *%ax
// CHECK: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %eax
// CHECK-NEXT:  jmpl	*%eax
// CHECK-NEXT:  .bundle_unlock
	
	jmp *%ecx
// CHECK: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock

	jmp *(%ecx)
// CHECK:	movl (%ecx), %ecx
// CHECK-NEXT: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock

	jmp *12(%ecx)
// CHECK:	movl 12(%ecx), %ecx
// CHECK-NEXT: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock

	jmp *-12(%ecx,%edx)
// CHECK:	movl -12(%ecx,%edx), %ecx
// CHECK-NEXT: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock

	jmp *-12(%ecx,%edx,4)
// CHECK:	movl -12(%ecx,%edx,4), %ecx
// CHECK-NEXT: 	.bundle_lock
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  jmpl	*%ecx
// CHECK-NEXT:  .bundle_unlock

