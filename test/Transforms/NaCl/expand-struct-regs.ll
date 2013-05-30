; RUN: opt %s -expand-struct-regs -S | FileCheck %s

%struct = type { i8, i32 }


define void @struct_load(%struct* %p, i8* %out0, i32* %out1) {
  %val = load %struct* %p
  %field0 = extractvalue %struct %val, 0
  %field1 = extractvalue %struct %val, 1
  store i8 %field0, i8* %out0
  store i32 %field1, i32* %out1
  ret void
}
; CHECK: define void @struct_load
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct* %p, i32 0, i32 0
; CHECK-NEXT: %val.field{{.*}} = load i8* %val.index{{.*}}, align 1
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct* %p, i32 0, i32 1
; CHECK-NEXT: %val.field{{.*}} = load i32* %val.index{{.*}}, align 1
; CHECK-NEXT: store i8 %val.field{{.*}}, i8* %out0
; CHECK-NEXT: store i32 %val.field{{.*}}, i32* %out1


define void @struct_store(%struct* %in_ptr, %struct* %out_ptr) {
  %val = load %struct* %in_ptr
  store %struct %val, %struct* %out_ptr
  ret void
}
; CHECK: define void @struct_store
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct* %in_ptr, i32 0, i32 0
; CHECK-NEXT: %val.field{{.*}} = load i8* %val.index{{.*}}, align 1
; CHECK-NEXT: %val.index{{.*}} = getelementptr %struct* %in_ptr, i32 0, i32 1
; CHECK-NEXT: %val.field{{.*}} = load i32* %val.index{{.*}}, align 1
; CHECK-NEXT: %out_ptr.index{{.*}} = getelementptr %struct* %out_ptr, i32 0, i32 0
; CHECK-NEXT: store i8 %val.field{{.*}}, i8* %out_ptr.index{{.*}}, align 1
; CHECK-NEXT: %out_ptr.index{{.*}} = getelementptr %struct* %out_ptr, i32 0, i32 1
; CHECK-NEXT: store i32 %val.field{{.*}}, i32* %out_ptr.index{{.*}}, align 1


; Ensure that the pass works correctly across basic blocks.
define void @across_basic_block(%struct* %in_ptr, %struct* %out_ptr) {
  %val = load %struct* %in_ptr
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
