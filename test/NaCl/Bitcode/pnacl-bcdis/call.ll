; Test call instructions

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; CHECK:         {{.*}}|    3: <7, 32>               |    @t0 = i32;
; CHECK-NEXT:    {{.*}}|    3: <2>                   |    @t1 = void;
; CHECK-NEXT:    {{.*}}|    3: <3>                   |    @t2 = float;


; Test simple calls
define void @foo(i32 %p0) {

; CHECK:               |                             |  %b0:

  %v0 = call i32 @bar(i32 %p0, i32 1)

; CHECK-NEXT:    {{.*}}|    3: <34, 0, 6, 2, 1>      |    %v0 = call i32 
; CHECK-NEXT:          |                             |        @f1(i32 %p0, i32 %c0);

  %v1 = call float @bam(i32 %p0)

; CHECK-NEXT:    {{.*}}|    3: <34, 0, 6, 3>         |    %v1 = call float @f2(i32 %p0);

  call void @huh()
  ret void

; CHECK-NEXT:    {{.*}}|    3: <34, 0, 6>            |    call void @f3();
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

define i32 @bar(i32 %p0, i32 %p1) {

; CHECK:               |                             |  %b0:

  %v0 = add i32 %p0, %p1
  ret i32 %v0
}

define float @bam(i32 %p0) {

; CHECK:               |                             |  %b0:

  %v0 = sitofp i32 %p0 to float
  ret float %v0
}

define void @huh() {

; CHECK:               |                             |  %b0:

  ret void
}


; Test indirect calls
define void @IndirectTests(i32 %p0, i32 %p1) {

; CHECK:               |                             |  %b0:

  ; Simulates indirect call to @foo
  %v0 = inttoptr i32 %p0 to void (i32)*
  call void %v0(i32 %p1)

; CHECK-NEXT:    {{.*}}|    3: <44, 0, 3, 1, 2>      |    call void %p0(i32 %p1);

  ; Simulates indirect call to @bar
  %v1 = inttoptr i32 %p0 to i32 (i32, i32)*
  %v2 = call i32 %v1(i32 %p1, i32 1)

; CHECK-NEXT:    {{.*}}|    3: <44, 0, 3, 0, 2, 1>   |    %v0 = call i32 
; CHECK-NEXT:          |                             |        %p0(i32 %p1, i32 %c0);

  ; Simulates indirect call to @bam
  %v3 = inttoptr i32 %p0 to float (i32)*
  %v4 = call float %v3(i32 %p1)

; CHECK-NEXT:    {{.*}}|    3: <44, 0, 4, 2, 3>      |    %v1 = call float %p0(i32 %p1);

  ; Simulates indirect call to @huh
  %v5 = inttoptr i32 %p0 to void ()*
  call void %v5()
  ret void

; CHECK-NEXT:    {{.*}}|    3: <44, 0, 5, 1>         |    call void %p0();
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

