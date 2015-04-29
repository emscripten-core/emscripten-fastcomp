; RUN: opt %s -expand-struct-regs -S | FileCheck %s
; RUN: opt %s -expand-struct-regs -S | FileCheck %s -check-prefix=CLEANUP

; These two instructions should not appear in the output:
; CLEANUP-NOT: extractvalue
; CLEANUP-NOT: insertvalue

target datalayout = "p:32:32:32"

%struct = type { i8, i32 }


define void @struct_load(%struct* %p, i8* %out0, i32* %out1) {
  %val = load %struct, %struct* %p
  %field0 = extractvalue %struct %val, 0
  %field1 = extractvalue %struct %val, 1
  store i8 %field0, i8* %out0
  store i32 %field1, i32* %out1
  ret void
}
; CHECK: define void @struct_load
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct, %struct* %p, i32 0, i32 0
; CHECK-NEXT: %val.field{{.*}} = load i8, i8* %val.index{{.*}}
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct, %struct* %p, i32 0, i32 1
; CHECK-NEXT: %val.field{{.*}} = load i32, i32* %val.index{{.*}}
; CHECK-NEXT: store i8 %val.field{{.*}}, i8* %out0
; CHECK-NEXT: store i32 %val.field{{.*}}, i32* %out1


define void @struct_store(%struct* %in_ptr, %struct* %out_ptr) {
  %val = load %struct, %struct* %in_ptr
  store %struct %val, %struct* %out_ptr
  ret void
}
; CHECK: define void @struct_store
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct, %struct* %in_ptr, i32 0, i32 0
; CHECK-NEXT: %val.field{{.*}} = load i8, i8* %val.index{{.*}}
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct, %struct* %in_ptr, i32 0, i32 1
; CHECK-NEXT: %val.field{{.*}} = load i32, i32* %val.index{{.*}}
; CHECK-NEXT: %out_ptr.index{{.*}} = getelementptr %struct, %struct* %out_ptr, i32 0, i32 0
; CHECK-NEXT: store i8 %val.field{{.*}}, i8* %out_ptr.index{{.*}}
; CHECK-NEXT: %out_ptr.index{{.*}} = getelementptr %struct, %struct* %out_ptr, i32 0, i32 1
; CHECK-NEXT: store i32 %val.field{{.*}}, i32* %out_ptr.index{{.*}}


; Ensure that the pass works correctly across basic blocks.
define void @across_basic_block(%struct* %in_ptr, %struct* %out_ptr) {
  %val = load %struct, %struct* %in_ptr
  br label %bb
bb:
  store %struct %val, %struct* %out_ptr
  ret void
}
; CHECK: define void @across_basic_block
; CHECK: load
; CHECK: load
; CHECK: bb:
; CHECK: store
; CHECK: store


define void @const_struct_store(%struct* %ptr) {
  store %struct { i8 99, i32 1234 }, %struct* %ptr
  ret void
}
; CHECK: define void @const_struct_store
; CHECK: store i8 99
; CHECK: store i32 1234


define void @struct_phi_node(%struct* %ptr) {
entry:
  %val = load %struct, %struct* %ptr
  br label %bb
bb:
  %phi = phi %struct [ %val, %entry ]
  ret void
}
; CHECK: bb:
; CHECK-NEXT: %phi.index{{.*}} = phi i8 [ %val.field{{.*}}, %entry ]
; CHECK-NEXT: %phi.index{{.*}} = phi i32 [ %val.field{{.*}}, %entry ]


define void @struct_phi_node_multiple_entry(i1 %arg, %struct* %ptr) {
entry:
  %val = load %struct, %struct* %ptr
  br i1 %arg, label %bb, label %bb
bb:
  %phi = phi %struct [ %val, %entry ], [ %val, %entry ]
  ret void
}
; CHECK: bb:
; CHECK-NEXT: %phi.index{{.*}} = phi i8 [ %val.field{{.*}}, %entry ], [ %val.field{{.*}}, %entry ]
; CHECK-NEXT: %phi.index{{.*}} = phi i32 [ %val.field{{.*}}, %entry ], [ %val.field{{.*}}, %entry ]


define void @struct_select_inst(i1 %cond, %struct* %ptr1, %struct* %ptr2) {
  %val1 = load %struct, %struct* %ptr1
  %val2 = load %struct, %struct* %ptr2
  %select = select i1 %cond, %struct %val1, %struct %val2
  ret void
}
; CHECK: define void @struct_select_inst
; CHECK: %select.index{{.*}} = select i1 %cond, i8 %val1.field{{.*}}, i8 %val2.field{{.*}}
; CHECK-NEXT: %select.index{{.*}} = select i1 %cond, i32 %val1.field{{.*}}, i32 %val2.field{{.*}}


define void @insert_and_extract(i8* %out0, i32* %out1) {
  %temp = insertvalue %struct undef, i8 100, 0
  %sval = insertvalue %struct %temp, i32 200, 1
  %field0 = extractvalue %struct %sval, 0
  %field1 = extractvalue %struct %sval, 1
  store i8 %field0, i8* %out0
  store i32 %field1, i32* %out1
  ret void
}
; CHECK: define void @insert_and_extract(i8* %out0, i32* %out1) {
; CHECK-NEXT: store i8 100, i8* %out0
; CHECK-NEXT: store i32 200, i32* %out1
; CHECK-NEXT: ret void


define i32 @extract_from_constant() {
  %ev = extractvalue %struct { i8 99, i32 888 }, 1
  ret i32 %ev
}
; CHECK: define i32 @extract_from_constant() {
; CHECK-NEXT: ret i32 888

define void @nested_structs() {
  %a1 = alloca i64
  %a2 = alloca i32
  %a3 = alloca { { i32, i64 } }
  %a = insertvalue { i32, i64 } undef, i32 5, 0
  %b = insertvalue { i32, i64 } %a, i64 6, 1
  %c = insertvalue { { i32, i64 } } undef, { i32, i64 } %b, 0
  %d = insertvalue { { { i32, i64 } }, i64 } undef, { { i32, i64 } } %c, 0
  %e = insertvalue { { { i32, i64 } }, i64 } undef, { i32, i64 } %b, 0, 0

  %f = extractvalue { { { i32, i64 } }, i64 } %d, 0, 0, 1
  %g = extractvalue { { { i32, i64 } }, i64 } %e, 0, 0, 0
  %h = extractvalue { { { i32, i64 } }, i64 } %e, 0
  store i64 %f, i64* %a1
  store i32 %g, i32* %a2
  store { { i32, i64 } } %h, { { i32, i64 } }* %a3
  ret void
}
; CHECK-LABEL: define void @nested_structs()
; CHECK-NEXT:    %a1 = alloca i64
; CHECK-NEXT:    %a2 = alloca i32
; CHECK-NEXT:    %a3 = alloca { { i32, i64 } }
; CHECK-NEXT:    store i64 6, i64* %a1
; CHECK-NEXT:    store i32 5, i32* %a2
; CHECK-NEXT:    %a3.index = getelementptr { { i32, i64 } }, { { i32, i64 } }* %a3, i32 0, i32 0
; CHECK-NEXT:    %a3.index.index = getelementptr { i32, i64 }, { i32, i64 }* %a3.index, i32 0, i32 0
; CHECK-NEXT:    store i32 5, i32* %a3.index.index
; CHECK-NEXT:    %a3.index.index1 = getelementptr { i32, i64 }, { i32, i64 }* %a3.index, i32 0, i32 1
; CHECK-NEXT:    store i64 6, i64* %a3.index.index1

define void @load_another_pass() {
  %a = alloca { { i8, i64 } }
  %b = load { { i8, i64 } }, { { i8, i64 } }* %a
  %c = load { { i8, i64 } }, { { i8, i64 } }* %a, align 16
  ret void
}
; CHECK-LABEL: define void @load_another_pass()
; CHECK:         %b.field.field = load i8, i8* %b.field.index
; CHECK:         %b.field.field{{.*}} = load i64, i64* %b.field.index{{.*}}
; CHECK:         %c.field.field = load i8, i8* %c.field.index, align 16
; CHECK:         %c.field.field{{.*}} = load i64, i64* %c.field.index{{.*}}, align 4

define void @store_another_pass() {
  %a = alloca { { i16, i64 } }
  store { { i16, i64 } } undef, { { i16, i64 } }* %a
  store { { i16, i64 } } undef, { { i16, i64 } }* %a, align 16
  ret void
}
; CHECK-LABEL: define void @store_another_pass()
; CHECK:         store i16 undef, i16* %a.index.index
; CHECK:         store i64 undef, i64* %a.index.index{{.*}}
; CHECK:         store i16 undef, i16* %a.index1.index, align 16
; CHECK:         store i64 undef, i64* %a.index1.index{{.*}}, align 4

define void @select_another_pass() {
  %a = load { { i8, i64 } }, { { i8, i64 } }* null
  %b = load { { i8, i64 } }, { { i8, i64 } }* null
  %c = select i1 undef, { { i8, i64 } } %a, { { i8, i64 } } %b
  store { { i8, i64 } } %c, { { i8, i64 } }* null
  ret void
}
; CHECK-LABEL: define void @select_another_pass()
; CHECK-NEXT:    %a.index = getelementptr { { i8, i64 } }, { { i8, i64 } }* null, i32 0, i32 0
; CHECK-NEXT:    %a.field.index = getelementptr { i8, i64 }, { i8, i64 }* %a.index, i32 0, i32 0
; CHECK-NEXT:    %a.field.field = load i8, i8* %a.field.index
; CHECK-NEXT:    %a.field.index2 = getelementptr { i8, i64 }, { i8, i64 }* %a.index, i32 0, i32 1
; CHECK-NEXT:    %a.field.field3 = load i64, i64* %a.field.index2
; CHECK-NEXT:    %b.index = getelementptr { { i8, i64 } }, { { i8, i64 } }* null, i32 0, i32 0
; CHECK-NEXT:    %b.field.index = getelementptr { i8, i64 }, { i8, i64 }* %b.index, i32 0, i32 0
; CHECK-NEXT:    %b.field.field = load i8, i8* %b.field.index
; CHECK-NEXT:    %b.field.index5 = getelementptr { i8, i64 }, { i8, i64 }* %b.index, i32 0, i32 1
; CHECK-NEXT:    %b.field.field6 = load i64, i64* %b.field.index5
; CHECK-NEXT:    %c.index.index = select i1 undef, i8 %a.field.field, i8 %b.field.field
; CHECK-NEXT:    %c.index.index11 = select i1 undef, i64 %a.field.field3, i64 %b.field.field6
; CHECK-NEXT:    %.index = getelementptr { { i8, i64 } }, { { i8, i64 } }* null, i32 0, i32 0
; CHECK-NEXT:    %.index.index = getelementptr { i8, i64 }, { i8, i64 }* %.index, i32 0, i32 0
; CHECK-NEXT:    store i8 %c.index.index, i8* %.index.index
; CHECK-NEXT:    %.index.index13 = getelementptr { i8, i64 }, { i8, i64 }* %.index, i32 0, i32 1
; CHECK-NEXT:    store i64 %c.index.index11, i64* %.index.index13
; CHECK-NEXT:    ret void

define void @phi_another_pass() {
entry:
  br i1 false, label %next, label %not_next

not_next:
  %a = alloca { { i64, i16 }, i8* }
  %b = load { { i64, i16 }, i8* }, { { i64, i16 }, i8* }* %a
  br label %next

next:
  %c = phi { { i64, i16 }, i8* } [ undef, %entry ], [ %b, %not_next ]
  store { { i64, i16 }, i8* } %c, { { i64, i16 }, i8* }* null
  ret void
}
; CHECK-LABEL: define void @phi_another_pass()
; CHECK:         %c.index.index = phi i64 [ undef, %entry ], [ %b.field.field, %not_next ]
; CHECK:         %c.index.index{{.*}} = phi i16 [ undef, %entry ], [ %b.field.field{{.*}}, %not_next ]
; CHECK:         %c.index{{.*}} = phi i8* [ undef, %entry ], [ %b.field{{.*}}, %not_next ]
