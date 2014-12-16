; RUN: opt %s -minsfi-sandbox-memory-accesses -S | FileCheck %s
; RUN: opt %s -minsfi-ptrsize=20 -minsfi-sandbox-memory-accesses -S \ 
; RUN:   | FileCheck %s -check-prefix=CHECK-MASK

!llvm.module.flags = !{!0}
!0 = metadata !{i32 1, metadata !"Debug Info Version", i32 2}

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

; CHECK:  @__sfi_memory_base = external global i64
; CHECK:  @__sfi_pointer_size = constant i32 32
; CHECK-MASK:  @__sfi_pointer_size = constant i32 20

declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture readonly, i32, i32, i1)
declare void @llvm.memmove.p0i8.p0i8.i32(i8* nocapture, i8* nocapture readonly, i32, i32, i1)
declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1)

declare i32 @llvm.nacl.atomic.load.i32(i32*, i32)
declare void @llvm.nacl.atomic.store.i32(i32, i32*, i32)
declare i32 @llvm.nacl.atomic.rmw.i32(i32, i32*, i32, i32)
declare i32 @llvm.nacl.atomic.cmpxchg.i32(i32*, i32, i32, i32, i32)

declare i64 @llvm.nacl.atomic.load.i64(i64*, i32)
declare void @llvm.nacl.atomic.store.i64(i64, i64*, i32)
declare i64 @llvm.nacl.atomic.rmw.i64(i32, i64*, i64, i32)
declare i64 @llvm.nacl.atomic.cmpxchg.i64(i64*, i64, i64, i32, i32)

declare void @llvm.nacl.atomic.fence(i32)
declare void @llvm.nacl.atomic.fence.all()
declare i1 @llvm.nacl.atomic.is.lock.free(i32, i8*)

define i32 @test_no_sandbox(i32 %x, i32 %y) {
  %sum = add i32 %x, %y
  ret i32 %sum
}

; CHECK-LABEL: define i32 @test_no_sandbox(i32 %x, i32 %y) {
; CHECK-NOT:     @__sfi_memory_base
; CHECK-NEXT:    %sum = add i32 %x, %y
; CHECK-NEXT:    ret i32 %sum
; CHECK-NEXT:  }

define i32 @test_load(i32* %ptr) {
  %val = load i32* %ptr
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_load(i32* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32* 
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i32 @test_load(i32* %ptr) {
; CHECK-MASK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-MASK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-MASK-NEXT:    %2 = and i32 %1, 1048575
; CHECK-MASK-NEXT:    %3 = zext i32 %2 to i64
; CHECK-MASK-NEXT:    %4 = add i64 %mem_base, %3
; CHECK-MASK-NEXT:    %5 = inttoptr i64 %4 to i32* 
; CHECK-MASK-NEXT:    %val = load i32* %5
; CHECK-MASK-NEXT:    ret i32 %val
; CHECK-MASK-NEXT:  }

define void @test_store(i32* %ptr) {
  store i32 1234, i32* %ptr
  ret void
}

; CHECK-LABEL: define void @test_store(i32* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32* 
; CHECK-NEXT:    store i32 1234, i32* %4
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define void @test_store(i32* %ptr) {
; CHECK-MASK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-MASK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-MASK-NEXT:    %2 = and i32 %1, 1048575
; CHECK-MASK-NEXT:    %3 = zext i32 %2 to i64
; CHECK-MASK-NEXT:    %4 = add i64 %mem_base, %3
; CHECK-MASK-NEXT:    %5 = inttoptr i64 %4 to i32* 
; CHECK-MASK-NEXT:    store i32 1234, i32* %5
; CHECK-MASK-NEXT:    ret void
; CHECK-MASK-NEXT:  }

define void @test_memcpy_32(i8* %dest, i8* %src, i32 %len) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 4, i1 false)
  ret void
}

; CHECK-LABEL: define void @test_memcpy_32(i8* %dest, i8* %src, i32 %len) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i8* %dest to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i8* 
; CHECK-NEXT:    %5 = ptrtoint i8* %src to i32
; CHECK-NEXT:    %6 = zext i32 %5 to i64
; CHECK-NEXT:    %7 = add i64 %mem_base, %6
; CHECK-NEXT:    %8 = inttoptr i64 %7 to i8* 
; CHECK-NEXT:    call void @llvm.memcpy.p0i8.p0i8.i32(i8* %4, i8* %8, i32 %len, i32 4, i1 false)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define void @test_memcpy_32(i8* %dest, i8* %src, i32 %len) {
; CHECK-MASK:         %11 = and i32 %len, 1048575
; CHECK-MASK-NEXT:    call void @llvm.memcpy.p0i8.p0i8.i32(i8* %5, i8* %10, i32 %11, i32 4, i1 false)

define void @test_memmove_32(i8* %dest, i8* %src, i32 %len) {
  call void @llvm.memmove.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 4, i1 false)
  ret void
}

; CHECK-LABEL: define void @test_memmove_32(i8* %dest, i8* %src, i32 %len) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i8* %dest to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i8* 
; CHECK-NEXT:    %5 = ptrtoint i8* %src to i32
; CHECK-NEXT:    %6 = zext i32 %5 to i64
; CHECK-NEXT:    %7 = add i64 %mem_base, %6
; CHECK-NEXT:    %8 = inttoptr i64 %7 to i8* 
; CHECK-NEXT:    call void @llvm.memmove.p0i8.p0i8.i32(i8* %4, i8* %8, i32 %len, i32 4, i1 false)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define void @test_memmove_32(i8* %dest, i8* %src, i32 %len) {
; CHECK-MASK:         %11 = and i32 %len, 1048575
; CHECK-MASK-NEXT:    call void @llvm.memmove.p0i8.p0i8.i32(i8* %5, i8* %10, i32 %11, i32 4, i1 false)

define void @test_memset_32(i8* %dest, i32 %len) {
  call void @llvm.memset.p0i8.i32(i8* %dest, i8 5, i32 %len, i32 4, i1 false)
  ret void
}

; CHECK-LABEL: define void @test_memset_32(i8* %dest, i32 %len) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i8* %dest to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i8* 
; CHECK-NEXT:    call void @llvm.memset.p0i8.i32(i8* %4, i8 5, i32 %len, i32 4, i1 false)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define void @test_memset_32(i8* %dest, i32 %len) {
; CHECK-MASK:         %6 = and i32 %len, 1048575
; CHECK-MASK-NEXT:    call void @llvm.memset.p0i8.i32(i8* %5, i8 5, i32 %6, i32 4, i1 false)

define i32 @test_atomic_load_32(i32* %ptr) {
  %val = call i32 @llvm.nacl.atomic.load.i32(i32* %ptr, i32 1)
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_atomic_load_32(i32* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32* 
; CHECK-NEXT:    %val = call i32 @llvm.nacl.atomic.load.i32(i32* %4, i32 1)
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i32 @test_atomic_load_32(i32* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define i64 @test_atomic_load_64(i64* %ptr) {
  %val = call i64 @llvm.nacl.atomic.load.i64(i64* %ptr, i32 1)
  ret i64 %val
}

; CHECK-LABEL: define i64 @test_atomic_load_64(i64* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i64* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i64* 
; CHECK-NEXT:    %val = call i64 @llvm.nacl.atomic.load.i64(i64* %4, i32 1)
; CHECK-NEXT:    ret i64 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i64 @test_atomic_load_64(i64* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define void @test_atomic_store_32(i32* %ptr) {
  call void @llvm.nacl.atomic.store.i32(i32 1234, i32* %ptr, i32 1)
  ret void
}

; CHECK-LABEL: define void @test_atomic_store_32(i32* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32* 
; CHECK-NEXT:    call void @llvm.nacl.atomic.store.i32(i32 1234, i32* %4, i32 1)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define void @test_atomic_store_32(i32* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define void @test_atomic_store_64(i64* %ptr) {
  call void @llvm.nacl.atomic.store.i64(i64 1234, i64* %ptr, i32 1)
  ret void
}

; CHECK-LABEL: define void @test_atomic_store_64(i64* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i64* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i64* 
; CHECK-NEXT:    call void @llvm.nacl.atomic.store.i64(i64 1234, i64* %4, i32 1)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define void @test_atomic_store_64(i64* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define i32 @test_atomic_rmw_32(i32* %ptr) {
  %val = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %ptr, i32 1234, i32 1)
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_atomic_rmw_32(i32* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32* 
; CHECK-NEXT:    %val = call i32 @llvm.nacl.atomic.rmw.i32(i32 1, i32* %4, i32 1234, i32 1)
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i32 @test_atomic_rmw_32(i32* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define i64 @test_atomic_rmw_64(i64* %ptr) {
  %val = call i64 @llvm.nacl.atomic.rmw.i64(i32 1, i64* %ptr, i64 1234, i32 1)
  ret i64 %val
}

; CHECK-LABEL: define i64 @test_atomic_rmw_64(i64* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i64* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i64* 
; CHECK-NEXT:    %val = call i64 @llvm.nacl.atomic.rmw.i64(i32 1, i64* %4, i64 1234, i32 1)
; CHECK-NEXT:    ret i64 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i64 @test_atomic_rmw_64(i64* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define i32 @test_atomic_cmpxchg_32(i32* %ptr) {
  %val = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %ptr, i32 0, i32 1, i32 1, i32 1)
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_atomic_cmpxchg_32(i32* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = call i32 @llvm.nacl.atomic.cmpxchg.i32(i32* %4, i32 0, i32 1, i32 1, i32 1)
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i32 @test_atomic_cmpxchg_32(i32* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define i64 @test_atomic_cmpxchg_64(i64* %ptr) {
  %val = call i64 @llvm.nacl.atomic.cmpxchg.i64(i64* %ptr, i64 0, i64 1, i32 1, i32 1)
  ret i64 %val
}

; CHECK-LABEL: define i64 @test_atomic_cmpxchg_64(i64* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i64* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i64*
; CHECK-NEXT:    %val = call i64 @llvm.nacl.atomic.cmpxchg.i64(i64* %4, i64 0, i64 1, i32 1, i32 1)
; CHECK-NEXT:    ret i64 %val
; CHECK-NEXT:  }

; CHECK-MASK-LABEL: define i64 @test_atomic_cmpxchg_64(i64* %ptr) {
; CHECK-MASK:         %2 = and i32 %1, 1048575

define void @test_atomic_fence() {
  call void @llvm.nacl.atomic.fence(i32 1)
  ret void
}

; CHECK-LABEL: define void @test_atomic_fence() {
; CHECK-NEXT:    call void @llvm.nacl.atomic.fence(i32 1)
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define void @test_atomic_fence_all() {
  call void @llvm.nacl.atomic.fence.all()
  ret void
}

; CHECK-LABEL: define void @test_atomic_fence_all() {
; CHECK-NEXT:    call void @llvm.nacl.atomic.fence.all()
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define i1 @test_atomic_is_lock_free(i8* %ptr) {
  %val = call i1 @llvm.nacl.atomic.is.lock.free(i32 4, i8* %ptr)
  ret i1 %val
}

; CHECK-LABEL: define i1 @test_atomic_is_lock_free(i8* %ptr) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i8* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i8*
; CHECK-NEXT:    %val = call i1 @llvm.nacl.atomic.is.lock.free(i32 4, i8* %4)
; CHECK-NEXT:    ret i1 %val
; CHECK-NEXT:  }

define void @test_bitcast_whitelisted(i32 %val) {
  %ptr = inttoptr i32 %val to i8*
  %ptr.bc = bitcast i8* %ptr to i32*
  ret void
}

; CHECK-LABEL: define void @test_bitcast_whitelisted(i32 %val) {
; CHECK-NEXT:    %ptr = inttoptr i32 %val to i8*
; CHECK-NEXT:    %ptr.bc = bitcast i8* %ptr to i32*
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

; -----------------------------------------------------------------------------
; Test the special case which optimizes sandboxing of the output of
; the ExpandGetElementPtr pass. 

; this won't get optimized because IntToPtr is not casting a result of an Add  
define i32 @test_no_opt__cast_not_add(i32 %ptr_int) {
  %ptr = inttoptr i32 %ptr_int to i32*
  %val = load i32* %ptr
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_no_opt__cast_not_add(i32 %ptr_int) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %ptr = inttoptr i32 %ptr_int to i32*
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; this won't get optimized because the cast is not from i32 
define i32 @test_no_opt__cast_not_32(i64 %ptr_int1) {
  %ptr_sum = add i64 %ptr_int1, 5
  %ptr = inttoptr i64 %ptr_sum to i32*
  %val = load i32* %ptr
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_no_opt__cast_not_32(i64 %ptr_int1) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %ptr_sum = add i64 %ptr_int1, 5
; CHECK-NEXT:    %ptr = inttoptr i64 %ptr_sum to i32*
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; this won't get optimized because the Add's 2nd operand is not a constant  
define i32 @test_no_opt__add_not_constant(i32 %ptr_int1, i32 %ptr_int2) {
  %ptr_sum = add i32 %ptr_int1, %ptr_int2  
  %ptr = inttoptr i32 %ptr_sum to i32*
  %val = load i32* %ptr
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_no_opt__add_not_constant(i32 %ptr_int1, i32 %ptr_int2) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %ptr_sum = add i32 %ptr_int1, %ptr_int2  
; CHECK-NEXT:    %ptr = inttoptr i32 %ptr_sum to i32*
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

; this won't get optimized because the Add's 2nd operand is not positive
define i32 @test_no_opt__add_not_positive(i32 %ptr_int) {
  %ptr_sum = add i32 %ptr_int, -5  
  %ptr = inttoptr i32 %ptr_sum to i32*
  %val = load i32* %ptr
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_no_opt__add_not_positive(i32 %ptr_int) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %ptr_sum = add i32 %ptr_int, -5  
; CHECK-NEXT:    %ptr = inttoptr i32 %ptr_sum to i32*
; CHECK-NEXT:    %1 = ptrtoint i32* %ptr to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

define i32 @test_opt_dont_remove_cast_if_used(i32 %ptr_int, i32 %replace) {
  %ptr_sum = add i32 %ptr_int, 5  
  %ptr = inttoptr i32 %ptr_sum to i32*
  %val = load i32* %ptr             ; %ptr is used later => keep cast
  store i32 %replace, i32* %ptr     ; %ptr not used any more => remove cast
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_opt_dont_remove_cast_if_used(i32 %ptr_int, i32 %replace) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = zext i32 %ptr_int to i64
; CHECK-NEXT:    %2 = add i64 %mem_base, %1
; CHECK-NEXT:    %3 = add i64 %2, 5
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    %5 = zext i32 %ptr_int to i64
; CHECK-NEXT:    %6 = add i64 %mem_base, %5
; CHECK-NEXT:    %7 = add i64 %6, 5
; CHECK-NEXT:    %8 = inttoptr i64 %7 to i32*
; CHECK-NEXT:    store i32 %replace, i32* %8
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }

define i32 @test_opt_dont_remove_add_if_used(i32 %ptr_int, i32 %replace) {
  %ptr_sum = add i32 %ptr_int, 5  
  %ptr = inttoptr i32 %ptr_sum to i32*
  store i32 %replace, i32* %ptr
  ret i32 %ptr_sum
}

; CHECK-LABEL: define i32 @test_opt_dont_remove_add_if_used(i32 %ptr_int, i32 %replace) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %ptr_sum = add i32 %ptr_int, 5  
; CHECK-NEXT:    %1 = zext i32 %ptr_int to i64
; CHECK-NEXT:    %2 = add i64 %mem_base, %1
; CHECK-NEXT:    %3 = add i64 %2, 5
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    store i32 %replace, i32* %4
; CHECK-NEXT:    ret i32 %ptr_sum
; CHECK-NEXT:  }


; ------------------------------------------------------------------------------
; Check that dbg symbols are preserved

define void @test_len_dbg(i8* %dest, i8* %src, i32 %len) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %dest, i8* %src, i32 %len, i32 4, i1 false), !dbg !1
  ret void
}

; CHECK-LABEL: define void @test_len_dbg(i8* %dest, i8* %src, i32 %len) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = ptrtoint i8* %dest to i32
; CHECK-NEXT:    %2 = zext i32 %1 to i64
; CHECK-NEXT:    %3 = add i64 %mem_base, %2
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i8*
; CHECK-NEXT:    %5 = ptrtoint i8* %src to i32
; CHECK-NEXT:    %6 = zext i32 %5 to i64
; CHECK-NEXT:    %7 = add i64 %mem_base, %6
; CHECK-NEXT:    %8 = inttoptr i64 %7 to i8*
; CHECK-NEXT:    call void @llvm.memcpy.p0i8.p0i8.i32(i8* %4, i8* %8, i32 %len, i32 4, i1 false), !dbg !1
; CHECK-NEXT:    ret void
; CHECK-NEXT:  }

define void @test_opt_dbg(i32 %ptr_int, i32 %replace) {
  %ptr_sum = add i32 %ptr_int, 5, !dbg !1
  %ptr = inttoptr i32 %ptr_sum to i32*, !dbg !2
  store i32 %replace, i32* %ptr, !dbg !3
  ret void, !dbg !4
}

; CHECK-LABEL: define void @test_opt_dbg(i32 %ptr_int, i32 %replace) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = zext i32 %ptr_int to i64
; CHECK-NEXT:    %2 = add i64 %mem_base, %1
; CHECK-NEXT:    %3 = add i64 %2, 5, !dbg !1
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*, !dbg !2
; CHECK-NEXT:    store i32 %replace, i32* %4, !dbg !3
; CHECK-NEXT:    ret void, !dbg !4
; CHECK-NEXT:  }

!1 = metadata !{i32 138, i32 0, metadata !1, null}
!2 = metadata !{i32 142, i32 0, metadata !2, null}
!3 = metadata !{i32 144, i32 0, metadata !3, null}
!4 = metadata !{i32 144, i32 0, metadata !4, null}
