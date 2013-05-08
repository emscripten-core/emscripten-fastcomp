; RUN: opt < %s -insert-divide-check -S | FileCheck -check-prefix=OPT %s

declare void @foo()

; Check for multiple divs that occur one block.
define i32 @twodivs_one_block(i32 %x, i32 %y) #0 {
entry:
  call void @foo()
  br label %divblock
divblock:
  %div1 = sdiv i32 %x, %y
  %div2 = sdiv i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: sdiv
; OPT: %1 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %1, label %divrem.by.zero1, label %guarded.divrem2
; OPT: guarded.divrem2:
; OPT-NEXT: sdiv
; OPT-NEXT: add
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
; OPT: divrem.by.zero1:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
  %sum = add i32 %div1, %div2
  ret i32 %sum
}

define i32 @twodivs_three_blocks(i32 %x, i32 %y) #0 {
entry:
  call void @foo()
  br label %divblock
divblock:
  %div1 = sdiv i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: sdiv
; OPT-NEXT: br label %exitblock
  br label %exitblock
exitblock:
  call void @foo()
  %div2 = sdiv i32 %x, %y
; OPT: %1 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %1, label %divrem.by.zero1, label %guarded.divrem2
; OPT: guarded.divrem2:
; OPT-NEXT: sdiv
; OPT-NEXT: add
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
; OPT: divrem.by.zero1:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
  %sum = add i32 %div1, %div2
  ret i32 %sum
}

; Check for divs that occur in blocks with multiple predecessors.
define i32 @onediv_two_predecessors(i32 %x, i32 %y) #0 {
entry:
  call void @foo()
  br label %divblock
divblock:
  %x1 = phi i32 [%x, %entry], [%x2, %divblock]
  %div1 = sdiv i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: sdiv
; OPT-NEXT: sub
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
  %x2 = sub i32 %x1, 1
  %p = icmp ne i32 %x2, 0
  br i1 %p, label %divblock, label %exitblock
exitblock:
  call void @foo()
  ret i32 %div1
}

