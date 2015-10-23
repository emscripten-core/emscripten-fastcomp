; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _add($0,$1,$2,$3) {
; CHECK:  $4 = (_i64Add(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @add(i64 %a, i64 %b) {
  %c = add i64 %a, %b
  ret i64 %c
}

; CHECK: function _sub($0,$1,$2,$3) {
; CHECK:  $4 = (_i64Subtract(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @sub(i64 %a, i64 %b) {
  %c = sub i64 %a, %b
  ret i64 %c
}

; CHECK: function _mul($0,$1,$2,$3) {
; CHECK:  $4 = (___muldi3(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @mul(i64 %a, i64 %b) {
  %c = mul i64 %a, %b
  ret i64 %c
}

; CHECK: function _sdiv($0,$1,$2,$3) {
; CHECK:  $4 = (___divdi3(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @sdiv(i64 %a, i64 %b) {
  %c = sdiv i64 %a, %b
  ret i64 %c
}

; CHECK: function _udiv($0,$1,$2,$3) {
; CHECK:  $4 = (___udivdi3(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @udiv(i64 %a, i64 %b) {
  %c = udiv i64 %a, %b
  ret i64 %c
}

; CHECK: function _srem($0,$1,$2,$3) {
; CHECK:  $4 = (___remdi3(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @srem(i64 %a, i64 %b) {
  %c = srem i64 %a, %b
  ret i64 %c
}

; CHECK: function _urem($0,$1,$2,$3) {
; CHECK:  $4 = (___uremdi3(($0|0),($1|0),($2|0),($3|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @urem(i64 %a, i64 %b) {
  %c = urem i64 %a, %b
  ret i64 %c
}

; CHECK: function _and($0,$1,$2,$3) {
; CHECK:  $4 = $0 & $2;
; CHECK:  $5 = $1 & $3;
; CHECK: }
define i64 @and(i64 %a, i64 %b) {
  %c = and i64 %a, %b
  ret i64 %c
}

; CHECK: function _or($0,$1,$2,$3) {
; CHECK:  $4 = $0 | $2;
; CHECK:  $5 = $1 | $3;
; CHECK: }
define i64 @or(i64 %a, i64 %b) {
  %c = or i64 %a, %b
  ret i64 %c
}

; CHECK: function _xor($0,$1,$2,$3) {
; CHECK:  $4 = $0 ^ $2;
; CHECK:  $5 = $1 ^ $3;
; CHECK: }
define i64 @xor(i64 %a, i64 %b) {
  %c = xor i64 %a, %b
  ret i64 %c
}

; CHECK: function _lshr($0,$1,$2,$3) {
; CHECK:  $4 = (_bitshift64Lshr(($0|0),($1|0),($2|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @lshr(i64 %a, i64 %b) {
  %c = lshr i64 %a, %b
  ret i64 %c
}

; CHECK: function _ashr($0,$1,$2,$3) {
; CHECK:  $4 = (_bitshift64Ashr(($0|0),($1|0),($2|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @ashr(i64 %a, i64 %b) {
  %c = ashr i64 %a, %b
  ret i64 %c
}

; CHECK: function _shl($0,$1,$2,$3) {
; CHECK:  $4 = (_bitshift64Shl(($0|0),($1|0),($2|0))|0);
; CHECK:  $5 = tempRet0;
; CHECK: }
define i64 @shl(i64 %a, i64 %b) {
  %c = shl i64 %a, %b
  ret i64 %c
}

; CHECK: function _icmp_eq($0,$1,$2,$3) {
; CHECK:  $4 = ($0|0)==($2|0);
; CHECK:  $5 = ($1|0)==($3|0);
; CHECK:  $6 = $4 & $5;
; CHECK: }
define i32 @icmp_eq(i64 %a, i64 %b) {
  %c = icmp eq i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: function _icmp_ne($0,$1,$2,$3) {
; CHECK:  $4 = ($0|0)!=($2|0);
; CHECK:  $5 = ($1|0)!=($3|0);
; CHECK:  $6 = $4 | $5;
; CHECK: }
define i32 @icmp_ne(i64 %a, i64 %b) {
  %c = icmp ne i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: function _icmp_slt($0,$1,$2,$3) {
; CHECK:  $4 = ($1|0)<($3|0);
; CHECK:  $5 = ($0>>>0)<($2>>>0);
; CHECK:  $6 = ($1|0)==($3|0);
; CHECK:  $7 = $6 & $5;
; CHECK:  $8 = $4 | $7;
; CHECK: }
define i32 @icmp_slt(i64 %a, i64 %b) {
  %c = icmp slt i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: function _icmp_ult($0,$1,$2,$3) {
; CHECK:  $4 = ($1>>>0)<($3>>>0);
; CHECK:  $5 = ($0>>>0)<($2>>>0);
; CHECK:  $6 = ($1|0)==($3|0);
; CHECK:  $7 = $6 & $5;
; CHECK:  $8 = $4 | $7;
; CHECK: }
define i32 @icmp_ult(i64 %a, i64 %b) {
  %c = icmp ult i64 %a, %b
  %d = zext i1 %c to i32
  ret i32 %d
}

; CHECK: function _load($a) {
; CHECK:  $0 = $a;
; CHECK:  $1 = $0;
; CHECK:  $2 = HEAP32[$1>>2]|0;
; CHECK:  $3 = (($0) + 4)|0;
; CHECK:  $4 = $3;
; CHECK:  $5 = HEAP32[$4>>2]|0;
; CHECK: }
define i64 @load(i64 *%a) {
  %c = load i64, i64* %a
  ret i64 %c
}

; CHECK: function _aligned_load($a) {
; CHECK:  $0 = $a;
; CHECK:  $1 = $0;
; CHECK:  $2 = HEAP32[$1>>2]|0;
; CHECK:  $3 = (($0) + 4)|0;
; CHECK:  $4 = $3;
; CHECK:  $5 = HEAP32[$4>>2]|0;
; CHECK: }
define i64 @aligned_load(i64 *%a) {
  %c = load i64, i64* %a, align 16
  ret i64 %c
}

; CHECK: function _store($a,$0,$1) {
; CHECK:  $2 = $a;
; CHECK:  $3 = $2;
; CHECK:  HEAP32[$3>>2] = $0;
; CHECK:  $4 = (($2) + 4)|0;
; CHECK:  $5 = $4;
; CHECK:  HEAP32[$5>>2] = $1;
; CHECK: }
define void @store(i64 *%a, i64 %b) {
  store i64 %b, i64* %a
  ret void
}

; CHECK: function _aligned_store($a,$0,$1) {
; CHECK:  $2 = $a;
; CHECK:  $3 = $2;
; CHECK:  HEAP32[$3>>2] = $0;
; CHECK:  $4 = (($2) + 4)|0;
; CHECK:  $5 = $4;
; CHECK:  HEAP32[$5>>2] = $1;
; CHECK: }
define void @aligned_store(i64 *%a, i64 %b) {
  store i64 %b, i64* %a, align 16
  ret void
}

; CHECK: function _call($0,$1) {
; CHECK:  $2 = (_foo(($0|0),($1|0))|0);
; CHECK: }
declare i64 @foo(i64 %arg)
define i64 @call(i64 %arg) {
  %ret = call i64 @foo(i64 %arg)
  ret i64 %ret
}

; CHECK: function _trunc($0,$1) {
; CHECK:   return ($0|0);
; CHECK: }
define i32 @trunc(i64 %x) {
  %y = trunc i64 %x to i32
  ret i32 %y
}

; CHECK: function _zext($x) {
; CHECK:  tempRet0 = (0);
; CHECL:  return ($x|0);
; CHECK: }
define i64 @zext(i32 %x) {
  %y = zext i32 %x to i64
  ret i64 %y
}

; CHECK: function _sext($x) {
; CHECK:  $0 = ($x|0)<(0);
; CHECK:  $1 = $0 << 31 >> 31;
; CHECK:  tempRet0 = ($1);
; CHECK:  return ($x|0);
; CHECK: }
define i64 @sext(i32 %x) {
  %y = sext i32 %x to i64
  ret i64 %y
}

; CHECK: function _unreachable_blocks($p) {
; CHECK: }
define void @unreachable_blocks(i64* %p) {
  ret void

dead:
  %t = load i64, i64* %p
  %s = add i64 %t, 1
  store i64 %s, i64* %p
  ret void
}

