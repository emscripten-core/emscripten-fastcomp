; RUN: opt %s -replace-ptrs-with-ints -S | FileCheck %s

target datalayout = "p:32:32:32"


%struct = type { i32, i32 }

declare %struct* @addr_taken_func(%struct*)

@addr_of_func = global %struct* (%struct*)* @addr_taken_func
; CHECK: @addr_of_func = global %struct* (%struct*)* bitcast (i32 (i32)* @addr_taken_func to %struct* (%struct*)*)

@blockaddr = global i8* blockaddress(@indirectbr, %l1)
; CHECK: @blockaddr = global i8* blockaddress(@indirectbr, %l1)


define i8* @pointer_arg(i8* %ptr, i64 %non_ptr) {
  ret i8* %ptr
}
; CHECK: define i32 @pointer_arg(i32 %ptr, i64 %non_ptr) {
; CHECK-NEXT: ret i32 %ptr
; CHECK-NEXT: }


declare i8* @declared_func(i8*, i64)
; CHECK: declare i32 @declared_func(i32, i64)


define void @self_reference_phi(i8* %ptr) {
entry:
  br label %loop
loop:
  %x = phi i8* [ %x, %loop ], [ %ptr, %entry ]
  br label %loop
}
; CHECK: define void @self_reference_phi(i32 %ptr) {
; CHECK: %x = phi i32 [ %x, %loop ], [ %ptr, %entry ]

; Self-referencing bitcasts are possible in unreachable basic blocks.
; It is not very likely that we will encounter this, but we handle it
; for completeness.
define void @self_reference_bitcast(i8** %dest) {
  ret void
unreachable_loop:
  store i8* %self_ref, i8** %dest
  %self_ref = bitcast i8* %self_ref to i8*
  store i8* %self_ref, i8** %dest
  br label %unreachable_loop
}
; CHECK: define void @self_reference_bitcast(i32 %dest) {
; CHECK: store i32 undef, i32* %dest.asptr
; CHECK: store i32 undef, i32* %dest.asptr

define void @circular_reference_bitcasts(i8** %dest) {
  ret void
unreachable_loop:
  store i8* %cycle1, i8** %dest
  %cycle1 = bitcast i8* %cycle2 to i8*
  %cycle2 = bitcast i8* %cycle1 to i8*
  br label %unreachable_loop
}
; CHECK: define void @circular_reference_bitcasts(i32 %dest) {
; CHECK: store i32 undef, i32* %dest.asptr

define void @circular_reference_inttoptr(i8** %dest) {
  ret void
unreachable_loop:
  %ptr = inttoptr i32 %int to i8*
  %int = ptrtoint i8* %ptr to i32
  store i8* %ptr, i8** %dest
  br label %unreachable_loop
}
; CHECK: define void @circular_reference_inttoptr(i32 %dest) {
; CHECK: store i32 undef, i32* %dest.asptr

define i8* @forwards_reference(%struct** %ptr) {
  br label %block1
block2:
  ; Forwards reference to %val.
  %cast = bitcast %struct* %val to i8*
  br label %block3
block1:
  %val = load %struct** %ptr
  br label %block2
block3:
  ; Backwards reference to a forwards reference that has already been
  ; resolved.
  ret i8* %cast
}
; CHECK: define i32 @forwards_reference(i32 %ptr) {
; CHECK-NEXT: br label %block1
; CHECK: block2:
; CHECK-NEXT: br label %block3
; CHECK: block1:
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i32*
; CHECK-NEXT: %val = load i32* %ptr.asptr
; CHECK-NEXT: br label %block2
; CHECK: block3:
; CHECK-NEXT: ret i32 %val


define i8* @phi_multiple_entry(i1 %arg, i8* %ptr) {
entry:
  br i1 %arg, label %done, label %done
done:
  %result = phi i8* [ %ptr, %entry ], [ %ptr, %entry ]
  ret i8* %result
}
; CHECK: define i32 @phi_multiple_entry(i1 %arg, i32 %ptr) {
; CHECK: %result = phi i32 [ %ptr, %entry ], [ %ptr, %entry ]


define i8* @select(i1 %cond, i8* %val1, i8* %val2) {
  %r = select i1 %cond, i8* %val1, i8* %val2
  ret i8* %r
}
; CHECK: define i32 @select(i1 %cond, i32 %val1, i32 %val2) {
; CHECK-NEXT: %r = select i1 %cond, i32 %val1, i32 %val2


define i32* @ptrtoint_same_size(i32* %ptr) {
  %a = ptrtoint i32* %ptr to i32
  %b = add i32 %a, 4
  %c = inttoptr i32 %b to i32*
  ret i32* %c
}
; CHECK: define i32 @ptrtoint_same_size(i32 %ptr) {
; CHECK-NEXT: %b = add i32 %ptr, 4
; CHECK-NEXT: ret i32 %b


define i32* @ptrtoint_different_size(i32* %ptr) {
  %a = ptrtoint i32* %ptr to i64
  %b = add i64 %a, 4
  %c = inttoptr i64 %b to i32*
  ret i32* %c
}
; CHECK: define i32 @ptrtoint_different_size(i32 %ptr) {
; CHECK-NEXT: %a = zext i32 %ptr to i64
; CHECK-NEXT: %b = add i64 %a, 4
; CHECK-NEXT: %c = trunc i64 %b to i32
; CHECK-NEXT: ret i32 %c


define i32* @pointer_bitcast(i64* %ptr) {
  %cast = bitcast i64* %ptr to i32*
  ret i32* %cast
}
; CHECK: define i32 @pointer_bitcast(i32 %ptr) {
; CHECK-NEXT: ret i32 %ptr

; Same-type non-pointer bitcasts happen to be left alone by this pass.
define i32 @no_op_bitcast(i32 %val) {
  %val2 = bitcast i32 %val to i32
  ret i32 %val2
}
; CHECK: define i32 @no_op_bitcast(i32 %val) {
; CHECK-NEXT: %val2 = bitcast i32 %val to i32

define i64 @kept_bitcast(double %d) {
  %i = bitcast double %d to i64
  ret i64 %i
}
; CHECK: define i64 @kept_bitcast(double %d) {
; CHECK-NEXT: %i = bitcast double %d to i64


define i32 @constant_pointer_null() {
  %val = ptrtoint i32* null to i32
  ret i32 %val
}
; CHECK: define i32 @constant_pointer_null() {
; CHECK-NEXT: ret i32 0

define i32 @constant_pointer_undef() {
  %val = ptrtoint i32* undef to i32
  ret i32 %val
}
; CHECK: define i32 @constant_pointer_undef() {
; CHECK-NEXT: ret i32 undef

define i16* @constant_pointer_null_load() {
  %val = load i16** null
  ret i16* %val
}
; CHECK: define i32 @constant_pointer_null_load() {
; CHECK-NEXT: %.asptr = inttoptr i32 0 to i32*
; CHECK-NEXT: %val = load i32* %.asptr


define i8 @load(i8* %ptr) {
  %x = load i8* %ptr
  ret i8 %x
}
; CHECK: define i8 @load(i32 %ptr) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i8*
; CHECK-NEXT: %x = load i8* %ptr.asptr

define void @store(i8* %ptr, i8 %val) {
  store i8 %val, i8* %ptr
  ret void
}
; CHECK: define void @store(i32 %ptr, i8 %val) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i8*
; CHECK-NEXT: store i8 %val, i8* %ptr.asptr


define i8* @load_ptr(i8** %ptr) {
  %x = load i8** %ptr
  ret i8* %x
}
; CHECK: define i32 @load_ptr(i32 %ptr) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i32*
; CHECK-NEXT: %x = load i32* %ptr.asptr

define void @store_ptr(i8** %ptr, i8* %val) {
  store i8* %val, i8** %ptr
  ret void
}
; CHECK: define void @store_ptr(i32 %ptr, i32 %val) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i32*
; CHECK-NEXT: store i32 %val, i32* %ptr.asptr


define i8 @load_attrs(i8* %ptr) {
  %x = load atomic volatile i8* %ptr seq_cst, align 128
  ret i8 %x
}
; CHECK: define i8 @load_attrs(i32 %ptr) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i8*
; CHECK-NEXT: %x = load atomic volatile i8* %ptr.asptr seq_cst, align 128

define void @store_attrs(i8* %ptr, i8 %val) {
  store atomic volatile i8 %val, i8* %ptr singlethread release, align 256
  ret void
}
; CHECK: define void @store_attrs(i32 %ptr, i8 %val) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i8*
; CHECK-NEXT: store atomic volatile i8 %val, i8* %ptr.asptr singlethread release, align 256


define i32 @cmpxchg(i32* %ptr, i32 %a, i32 %b) {
  %r = cmpxchg i32* %ptr, i32 %a, i32 %b seq_cst
  ret i32 %r
}
; CHECK: define i32 @cmpxchg(i32 %ptr, i32 %a, i32 %b) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i32*
; CHECK-NEXT: %r = cmpxchg i32* %ptr.asptr, i32 %a, i32 %b seq_cst

define i32 @atomicrmw(i32* %ptr, i32 %x) {
  %r = atomicrmw add i32* %ptr, i32 %x seq_cst
  ret i32 %r
}
; CHECK: define i32 @atomicrmw(i32 %ptr, i32 %x) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i32*
; CHECK-NEXT: %r = atomicrmw add i32* %ptr.asptr, i32 %x seq_cst


define i8* @indirect_call(i8* (i8*)* %func, i8* %arg) {
  %result = call i8* %func(i8* %arg)
  ret i8* %result
}
; CHECK: define i32 @indirect_call(i32 %func, i32 %arg) {
; CHECK-NEXT: %func.asptr = inttoptr i32 %func to i32 (i32)*
; CHECK-NEXT: %result = call i32 %func.asptr(i32 %arg)
; CHECK-NEXT: ret i32 %result


; Test forwards reference
define i8* @direct_call1(i8* %arg) {
  %result = call i8* @direct_call2(i8* %arg)
  ret i8* %result
}
; CHECK: define i32 @direct_call1(i32 %arg) {
; CHECK-NEXT: %result = call i32 @direct_call2(i32 %arg)
; CHECK-NEXT: ret i32 %result

; Test backwards reference
define i8* @direct_call2(i8* %arg) {
  %result = call i8* @direct_call1(i8* %arg)
  ret i8* %result
}
; CHECK: define i32 @direct_call2(i32 %arg) {
; CHECK-NEXT: %result = call i32 @direct_call1(i32 %arg)
; CHECK-NEXT: ret i32 %result


@var = global i32 0

define i32* @get_addr_of_global() {
  ret i32* @var
}
; CHECK: define i32 @get_addr_of_global() {
; CHECK-NEXT: %expanded = ptrtoint i32* @var to i32
; CHECK-NEXT: ret i32 %expanded

define %struct* (%struct*)* @get_addr_of_func() {
  ret %struct* (%struct*)* @addr_taken_func
}
; CHECK: define i32 @get_addr_of_func() {
; CHECK-NEXT: %expanded = ptrtoint i32 (i32)* @addr_taken_func to i32
; CEHCK-NEXT: ret i32 %expanded


define i32 @load_global() {
  %val = load i32* @var
  ret i32 %val
}
; CHECK: define i32 @load_global() {
; CHECK-NEXT: %val = load i32* @var
; CHECK-NEXT: ret i32 %val

define i16 @load_global_bitcast() {
  %ptr = bitcast i32* @var to i16*
  %val = load i16* %ptr
  ret i16 %val
}
; CHECK: define i16 @load_global_bitcast() {
; CHECK-NEXT: %var.bc = bitcast i32* @var to i16*
; CHECK-NEXT: %val = load i16* %var.bc
; CHECK-NEXT: ret i16 %val


declare void @receive_alloca(%struct* %ptr)

define void @alloca_fixed() {
  %buf = alloca %struct, align 128
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK: define void @alloca_fixed() {
; CHECK-NEXT: %buf = alloca [8 x i8], align 128
; CHECK-NEXT: %buf.asint = ptrtoint [8 x i8]* %buf to i32
; CHECK-NEXT: call void @receive_alloca(i32 %buf.asint)

define void @alloca_variable(i32 %size) {
  %buf = alloca %struct, i32 %size
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK: define void @alloca_variable(i32 %size) {
; CHECK-NEXT: %buf = alloca [8 x i8], i32 %size
; CHECK-NEXT: %buf.asint = ptrtoint [8 x i8]* %buf to i32
; CHECK-NEXT: call void @receive_alloca(i32 %buf.asint)

define void @alloca_alignment_i32() {
  %buf = alloca i32
  ret void
}
; CHECK: void @alloca_alignment_i32() {
; CHECK-NEXT: alloca [4 x i8], align 4

define void @alloca_alignment_double() {
  %buf = alloca double
  ret void
}
; CHECK: void @alloca_alignment_double() {
; CHECK-NEXT: alloca [8 x i8], align 8

define void @alloca_lower_alignment() {
  %buf = alloca i32, align 1
  ret void
}
; CHECK: void @alloca_lower_alignment() {
; CHECK-NEXT: alloca [4 x i8], align 1


; This tests for a bug in which, when processing the store's %buf2
; operand, ReplacePtrsWithInts accidentally strips off the ptrtoint
; cast that it previously introduced for the 'alloca', causing an
; internal sanity check to fail.
define void @alloca_cast_stripping() {
  %buf = alloca i32
  %buf1 = ptrtoint i32* %buf to i32
  %buf2 = inttoptr i32 %buf1 to i32*
  store i32 0, i32* %buf2
  ret void
}
; CHECK: define void @alloca_cast_stripping() {
; CHECK-NEXT: %buf = alloca [4 x i8]
; CHECK-NEXT: %buf.bc = bitcast [4 x i8]* %buf to i32*
; CHECK-NEXT: store i32 0, i32* %buf.bc


define i1 @compare(i8* %ptr1, i8* %ptr2) {
  %cmp = icmp ult i8* %ptr1, %ptr2
  ret i1 %cmp
}
; CHECK: define i1 @compare(i32 %ptr1, i32 %ptr2) {
; CHECK-NEXT: %cmp = icmp ult i32 %ptr1, %ptr2


declare i8* @llvm.some.intrinsic(i8* %ptr)

define i8* @preserve_intrinsic_type(i8* %ptr) {
  %result = call i8* @llvm.some.intrinsic(i8* %ptr)
  ret i8* %result
}
; CHECK: define i32 @preserve_intrinsic_type(i32 %ptr) {
; CHECK-NEXT: %ptr.asptr = inttoptr i32 %ptr to i8*
; CHECK-NEXT: %result = call i8* @llvm.some.intrinsic(i8* %ptr.asptr)
; CHECK-NEXT: %result.asint = ptrtoint i8* %result to i32
; CHECK-NEXT: ret i32 %result.asint


; Just check that the pass does not crash on inline asm.
define i16* @inline_asm1(i8* %ptr) {
  %val = call i16* asm "foo", "=r,r"(i8* %ptr)
  ret i16* %val
}

define i16** @inline_asm2(i8** %ptr) {
  %val = call i16** asm "foo", "=r,r"(i8** %ptr)
  ret i16** %val
}


declare void @llvm.dbg.declare(metadata, metadata)
declare void @llvm.dbg.value(metadata, i64, metadata)

define void @debug_declare(i32 %val) {
  ; We normally expect llvm.dbg.declare to be used on an alloca.
  %var = alloca i32
  tail call void @llvm.dbg.declare(metadata !{i32* %var}, metadata !{})
  tail call void @llvm.dbg.declare(metadata !{i32 %val}, metadata !{})
  ret void
}
; CHECK: define void @debug_declare(i32 %val) {
; CHECK-NEXT: %var = alloca [4 x i8]
; CHECK-NEXT: call void @llvm.dbg.declare(metadata !{[4 x i8]* %var}, metadata !0)
; This case is currently not converted.
; CHECK-NEXT: call void @llvm.dbg.declare(metadata !{null}, metadata !0)
; CHECK-NEXT: ret void

; For now, debugging info for values is lost.  replaceAllUsesWith()
; does not work for metadata references -- it converts them to nulls.
; This makes dbg.value too tricky to handle for now.
define void @debug_value(i32 %val, i8* %ptr) {
  tail call void @llvm.dbg.value(metadata !{i32 %val}, i64 1, metadata !{})
  tail call void @llvm.dbg.value(metadata !{i8* %ptr}, i64 2, metadata !{})
  ret void
}
; CHECK: define void @debug_value(i32 %val, i32 %ptr) {
; CHECK-NEXT: call void @llvm.dbg.value(metadata !{null}, i64 1, metadata !0)
; CHECK-NEXT: call void @llvm.dbg.value(metadata !{null}, i64 2, metadata !0)
; CHECK-NEXT: ret void


declare void @llvm.lifetime.start(i64 %size, i8* %ptr)
declare void @llvm.invariant.start(i64 %size, i8* %ptr)
declare void @llvm.invariant.end(i64 %size, i8* %ptr)

; GVN can introduce the following horrible corner case of a lifetime
; marker referencing a PHI node.  But we convert the phi to i32 type,
; and lifetime.start doesn't work on an inttoptr converting an i32 phi
; to a pointer.  Because of this, we just strip out all lifetime
; markers.

define void @alloca_lifetime_via_phi() {
entry:
  %buf = alloca i8
  br label %block
block:
  %phi = phi i8* [ %buf, %entry ]
  call void @llvm.lifetime.start(i64 -1, i8* %phi)
  ret void
}
; CHECK: define void @alloca_lifetime_via_phi() {
; CHECK: %phi = phi i32 [ %buf.asint, %entry ]
; CHECK-NEXT: ret void

define void @alloca_lifetime() {
  %buf = alloca i8
  call void @llvm.lifetime.start(i64 -1, i8* %buf)
  ret void
}
; CHECK: define void @alloca_lifetime() {
; CHECK-NEXT: %buf = alloca [1 x i8]
; CHECK-NEXT: ret void

define void @alloca_lifetime_via_bitcast() {
  %buf = alloca i32
  %buf_cast = bitcast i32* %buf to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %buf_cast)
  ret void
}
; CHECK: define void @alloca_lifetime_via_bitcast() {
; CHECK-NEXT: %buf = alloca [4 x i8]
; CHECK-NEXT: ret void

define void @strip_invariant_markers() {
  %buf = alloca i8
  call void @llvm.invariant.start(i64 1, i8* %buf)
  call void @llvm.invariant.end(i64 1, i8* %buf)
  ret void
}
; CHECK: define void @strip_invariant_markers() {
; CHECK-NEXT: %buf = alloca [1 x i8]
; CHECK-NEXT: ret void


; "nocapture" and "noalias" only apply to pointers, so must be stripped.
define void @nocapture_attr(i8* nocapture noalias %ptr) {
  ret void
}
; CHECK: define void @nocapture_attr(i32 %ptr) {

; "nounwind" should be preserved.
define void @nounwind_func_attr() nounwind {
  ret void
}
; CHECK: define void @nounwind_func_attr() [[NOUNWIND:#[0-9]+]] {

define void @nounwind_call_attr() {
  call void @nounwind_func_attr() nounwind
  ret void
}
; CHECK: define void @nounwind_call_attr() {
; CHECK: call void @nounwind_func_attr() {{.*}}[[NOUNWIND]]

define fastcc void @fastcc_func() {
  ret void
}
; CHECK: define fastcc void @fastcc_func() {

define void @fastcc_call() {
  call fastcc void @fastcc_func()
  ret void
}
; CHECK: define void @fastcc_call() {
; CHECK-NEXT: call fastcc void @fastcc_func()


; Just check that the pass does not crash on getelementptr.  (The pass
; should not depend unnecessarily on ExpandGetElementPtr having been
; run.)
define i8* @getelementptr(i8* %ptr) {
  %gep = getelementptr i8* %ptr, i32 10
  ret i8* %gep
}

; Just check that the pass does not crash on va_arg.
define i32* @va_arg(i8* %valist) {
  %r = va_arg i8* %valist, i32*
  ret i32* %r
}


define void @indirectbr(i8* %addr) {
  indirectbr i8* %addr, [ label %l1, label %l2 ]
l1:
  ret void
l2:
  ret void
}
; CHECK: define void @indirectbr(i32 %addr) {
; CHECK-NEXT: %addr.asptr = inttoptr i32 %addr to i8*
; CHECK-NEXT: indirectbr i8* %addr.asptr, [label %l1, label %l2]


define i8* @invoke(i8* %val) {
  %result = invoke i8* @direct_call1(i8* %val)
      to label %cont unwind label %lpad
cont:
  ret i8* %result
lpad:
  %lp = landingpad { i8*, i32 } personality void (i8*)* @personality_func cleanup
  %p = extractvalue { i8*, i32 } %lp, 0
  %s = insertvalue { i8*, i32 } %lp, i8* %val, 0
  ret i8* %p
}
; CHECK: define i32 @invoke(i32 %val) {
; CHECK-NEXT: %result = invoke i32 @direct_call1(i32 %val)
; CHECK-NEXT:         to label %cont unwind label %lpad
; CHECK: %lp = landingpad { i8*, i32 } personality void (i8*)* bitcast (void (i32)* @personality_func to void (i8*)*)
; CHECK: %p = extractvalue { i8*, i32 } %lp, 0
; CHECK-NEXT: %p.asint = ptrtoint i8* %p to i32
; CHECK-NEXT: %val.asptr = inttoptr i32 %val to i8*
; CHECK-NEXT: %s = insertvalue { i8*, i32 } %lp, i8* %val.asptr, 0
; CHECK-NEXT: ret i32 %p.asint

define void @personality_func(i8* %arg) {
  ret void
}


declare i32 @llvm.eh.typeid.for(i8*)

@typeid = global i32 0

; The argument here must be left as a bitcast, otherwise the backend
; rejects it.
define void @typeid_for() {
  %bc = bitcast i32* @typeid to i8*
  call i32 @llvm.eh.typeid.for(i8* %bc)
  ret void
}
; CHECK: define void @typeid_for() {
; CHECK-NEXT: %typeid.bc = bitcast i32* @typeid to i8*
; CHECK-NEXT: call i32 @llvm.eh.typeid.for(i8* %typeid.bc)


; CHECK: attributes {{.*}}[[NOUNWIND]] = { nounwind }
