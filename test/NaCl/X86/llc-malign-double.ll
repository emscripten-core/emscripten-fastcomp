; RUN: pnacl-llc -mtriple=i386-pc-linux -filetype=asm %s -o - | FileCheck %s
; RUN: pnacl-llc -mtriple=i386-pc-linux -malign-double -filetype=asm %s -o -| FileCheck %s --check-prefix=MALIGN
; RUN: pnacl-llc -mtriple=i386-unknown-nacl -filetype=asm %s -o -| FileCheck %s --check-prefix=MALIGN

; Test that the -malign-double flag causes i64 and f64 types to have alignment
; 8 instead of 4.

%structi64 = type { i32, i64 }

define i32 @check_i64_size() {
; CHECK-LABEL: check_i64_size:
  %starti = ptrtoint %structi64* null to i32
  %endp = getelementptr %structi64* null, i32 1
  %endi = ptrtoint %structi64* %endp to i32
  %diff = sub i32 %endi, %starti
; CHECK: movl $12, %eax
; MALIGN: movl $16, %eax
  ret i32 %diff
}

define i32 @check_i64_offset() {
; CHECK-LABEL: check_i64_offset:
  %starti = ptrtoint %structi64* null to i32
  %endp = getelementptr %structi64* null, i32 0, i32 1
  %endi = ptrtoint i64* %endp to i32
  %diff = sub i32 %endi, %starti
; CHECK: movl $4, %eax
; MALIGN: movl $8, %eax
  ret i32 %diff
}

%structf64 = type { i32, double }

define i32 @check_f64_size() {
; CHECK-LABEL: check_f64_size:
  %starti = ptrtoint %structf64* null to i32
  %endp = getelementptr %structf64* null, i32 1
  %endi = ptrtoint %structf64* %endp to i32
  %diff = sub i32 %endi, %starti
; CHECK: movl $12, %eax
; MALIGN: movl $16, %eax
  ret i32 %diff
}

define i32 @check_f64_offset() {
; CHECK-LABEL: check_f64_offset:
  %starti = ptrtoint %structf64* null to i32
  %endp = getelementptr %structf64* null, i32 0, i32 1
  %endi = ptrtoint double* %endp to i32
  %diff = sub i32 %endi, %starti
; CHECK: movl $4, %eax
; MALIGN: movl $8, %eax
  ret i32 %diff
}
