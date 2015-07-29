// RUN: llvm-mc -filetype asm -triple i386-unknown-nacl %s | FileCheck %s
.scratch %ecx

// Tests if the call instruction is expanded correctly.
	
	call foo
foo:
// CHECK:	calll foo
	
	call *%eax
// CHECK: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %eax
// CHECK-NEXT:  calll	*%eax
// CHECK-NEXT:  .bundle_unlock

	call *%ax
// CHECK: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %eax
// CHECK-NEXT:  calll	*%eax
// CHECK-NEXT:  .bundle_unlock
	
	call *%ecx
// CHECK: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  calll	*%ecx
// CHECK-NEXT:  .bundle_unlock

	call *(%ecx)
// CHECK:	movl (%ecx), %ecx
// CHECK-NEXT: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  calll	*%ecx
// CHECK-NEXT:  .bundle_unlock

	call *12(%ecx)
// CHECK:	movl 12(%ecx), %ecx
// CHECK-NEXT: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  calll	*%ecx
// CHECK-NEXT:  .bundle_unlock

	call *-12(%ecx,%edx)
// CHECK:	movl -12(%ecx,%edx), %ecx
// CHECK-NEXT: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  calll	*%ecx
// CHECK-NEXT:  .bundle_unlock

	call *-12(%ecx,%edx,4)
// CHECK:	movl -12(%ecx,%edx,4), %ecx
// CHECK-NEXT: 	.bundle_lock align_to_end
// CHECK-NEXT:  andl	$-32, %ecx
// CHECK-NEXT:  calll	*%ecx
// CHECK-NEXT:  .bundle_unlock

