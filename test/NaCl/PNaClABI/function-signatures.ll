; RUN: not pnacl-abicheck < %s | FileCheck %s

; Test type-checking of function signatures.

; CHECK: Function f_arg_i1 has disallowed type: void (i1)
; CHECK: Function f_ret_i1 has disallowed type: i1 ()
define internal void @f_arg_i1(i1 %a) {
  ret void
}
define internal i1 @f_ret_i1() {
  ret i1 undef
}

; CHECK: Function f_arg_i8 has disallowed type: void (i8)
; CHECK: Function f_ret_i8 has disallowed type: i8 ()
define internal void @f_arg_i8(i8 %a) {
  ret void
}
define internal i8 @f_ret_i8() {
  ret i8 undef
}

; CHECK: Function f_arg_i16 has disallowed type: void (i16)
; CHECK: Function f_ret_i16 has disallowed type: i16 ()
define internal void @f_arg_i16(i16 %a) {
  ret void
}
define internal i16 @f_ret_i16() {
  ret i16 undef
}

; CHECK-NOT: f_{{[a-z]+}}_i32 {{.*}} disallowed
define internal void @f_arg_i32(i32 %a) {
  ret void
}
define internal i32 @f_ret_i32() {
  ret i32 undef
}

; CHECK-NOT: f_{{[a-z]+}}_i64 {{.*}} disallowed
define internal void @f_arg_i64(i64 %a) {
  ret void
}
define internal i64 @f_ret_i64() {
  ret i64 undef
}

; CHECK: Function f_arg_i128 has disallowed type: void (i128)
; CHECK: Function f_ret_i128 has disallowed type: i128 ()
define internal void @f_arg_i128(i128 %a) {
  ret void
}
define internal i128 @f_ret_i128() {
  ret i128 undef
}

; CHECK-NOT: f_{{[a-z]+}}_float {{.*}} disallowed
define internal void @f_arg_float(float %a) {
  ret void
}
define internal float @f_ret_float() {
  ret float undef
}

; CHECK-NOT: f_{{[a-z]+}}_double {{.*}} disallowed
define internal void @f_arg_double(double %a) {
  ret void
}
define internal double @f_ret_double() {
  ret double undef
}

; CHECK: Function f_arg_1xi1 has disallowed type: void (<1 x i1>)
; CHECK: Function f_ret_1xi1 has disallowed type: <1 x i1> ()
define internal void @f_arg_1xi1(<1 x i1> %a) {
  ret void
}
define internal <1 x i1> @f_ret_1xi1() {
  ret <1 x i1> undef
}

; CHECK: Function f_arg_2xi1 has disallowed type: void (<2 x i1>)
; CHECK: Function f_ret_2xi1 has disallowed type: <2 x i1> ()
define internal void @f_arg_2xi1(<2 x i1> %a) {
  ret void
}
define internal <2 x i1> @f_ret_2xi1() {
  ret <2 x i1> undef
}

; CHECK-NOT: f_{{[a-z]+}}_4xi1 {{.*}} disallowed
define internal void @f_arg_4xi1(<4 x i1> %a) {
  ret void
}
define internal <4 x i1> @f_ret_4xi1() {
  ret <4 x i1> undef
}

; CHECK-NOT: f_{{[a-z]+}}_8xi1 {{.*}} disallowed
define internal void @f_arg_8xi1(<8 x i1> %a) {
  ret void
}
define internal <8 x i1> @f_ret_8xi1() {
  ret <8 x i1> undef
}

; CHECK-NOT: f_{{[a-z]+}}_16xi1 {{.*}} disallowed
define internal void @f_arg_16xi1(<16 x i1> %a) {
  ret void
}
define internal <16 x i1> @f_ret_16xi1() {
  ret <16 x i1> undef
}

; CHECK: Function f_arg_32xi1 has disallowed type: void (<32 x i1>)
; CHECK: Function f_ret_32xi1 has disallowed type: <32 x i1> ()
define internal void @f_arg_32xi1(<32 x i1> %a) {
  ret void
}
define internal <32 x i1> @f_ret_32xi1() {
  ret <32 x i1> undef
}

; CHECK: Function f_arg_64xi1 has disallowed type: void (<64 x i1>)
; CHECK: Function f_ret_64xi1 has disallowed type: <64 x i1> ()
define internal void @f_arg_64xi1(<64 x i1> %a) {
  ret void
}
define internal <64 x i1> @f_ret_64xi1() {
  ret <64 x i1> undef
}

; CHECK-NOT: f_{{[a-z]+}}_16xi8 {{.*}} disallowed
define internal void @f_arg_16xi8(<16 x i8> %a) {
  ret void
}
define internal <16 x i8> @f_ret_16xi8() {
  ret <16 x i8> undef
}

; CHECK-NOT: f_{{[a-z]+}}_8xi16 {{.*}} disallowed
define internal void @f_arg_8xi16(<8 x i16> %a) {
  ret void
}
define internal <8 x i16> @f_ret_8xi16() {
  ret <8 x i16> undef
}

; CHECK-NOT: f_{{[a-z]+}}_4xi32 {{.*}} disallowed
define internal void @f_arg_4xi32(<4 x i32> %a) {
  ret void
}
define internal <4 x i32> @f_ret_4xi32() {
  ret <4 x i32> undef
}

; CHECK: Function f_arg_2xi64 has disallowed type: void (<2 x i64>)
; CHECK: Function f_ret_2xi64 has disallowed type: <2 x i64> ()
define internal void @f_arg_2xi64(<2 x i64> %a) {
  ret void
}
define internal <2 x i64> @f_ret_2xi64() {
  ret <2 x i64> undef
}

; CHECK-NOT: f_{{[a-z]+}}_4xfloat {{.*}} disallowed
define internal void @f_arg_4xfloat(<4 x float> %a) {
  ret void
}
define internal <4 x float> @f_ret_4xfloat() {
  ret <4 x float> undef
}

; CHECK: Function f_arg_2xdouble has disallowed type: void (<2 x double>)
; CHECK: Function f_ret_2xdouble has disallowed type: <2 x double> ()
define internal void @f_arg_2xdouble(<2 x double> %a) {
  ret void
}
define internal <2 x double> @f_ret_2xdouble() {
  ret <2 x double> undef
}
