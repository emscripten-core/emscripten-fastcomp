/*
  Object file built using:
  pnacl-clang -S -O2 -emit-llvm -o pnacl-avoids-r11-x86-64.ll \
      pnacl-avoids-r11-x86-64.c
  Then the comments below should be pasted into the .ll file,
  replacing "RUNxxx" with "RUN".

; The NACLON test verifies that %r11 and %r11d are not used except as
; part of the return sequence.
;
; RUNxxx: pnacl-llc -O2 -mtriple=x86_64-none-nacl < %s | \
; RUNxxx:     FileCheck %s --check-prefix=NACLON
;
; The NACLOFF test verifies that %r11 would normally be used if PNaCl
; weren't reserving r11 for its own uses, to be sure NACLON is a
; valid test.
;
; RUNxxx: pnacl-llc -O2 -mtriple=x86_64-linux < %s | \
; RUNxxx:     FileCheck %s --check-prefix=NACLOFF
;
; NACLON: RegisterPressure:
; NACLON-NOT: %r11
; NACLON: popq %r11
; NACLON: nacljmp %r11, %r15
;
; NACLOFF: RegisterPressure:
; NACLOFF: %r11
; NACLOFF: ret

*/

// Function RegisterPressure() tries to induce maximal integer
// register pressure in a ~16 register machine, for both scratch and
// preserved registers.  Repeated calls to Use() are designed to
// use all the preserved registers.  The calculations on the local
// variables between function calls are designed to use all the
// scratch registers.

void RegisterPressure(void)
{
  extern void Use(int, int, int, int, int, int, int, int,
                  int, int, int, int, int, int, int, int);
  extern int GetValue(void);
  extern volatile int v1a, v1b, v2a, v2b, v3a, v3b, v4a, v4b;

  int i00 = GetValue();
  int i01 = GetValue();
  int i02 = GetValue();
  int i03 = GetValue();
  int i04 = GetValue();
  int i05 = GetValue();
  int i06 = GetValue();
  int i07 = GetValue();
  int i08 = GetValue();
  int i09 = GetValue();
  int i10 = GetValue();
  int i11 = GetValue();
  int i12 = GetValue();
  int i13 = GetValue();
  int i14 = GetValue();
  int i15 = GetValue();

  Use(i00, i01, i02, i03, i04, i05, i06, i07,
      i08, i09, i10, i11, i12, i13, i14, i15);
  Use(i00, i01, i02, i03, i04, i05, i06, i07,
      i08, i09, i10, i11, i12, i13, i14, i15);
  v1a = i00 + i01 + i02 + i03 + i04 + i05 + i06 + i07;
  v1b = i08 + i09 + i10 + i11 + i12 + i13 + i14 + i15;
  v2a = i00 + i01 + i02 + i03 + i08 + i09 + i10 + i11;
  v2b = i04 + i05 + i06 + i07 + i12 + i13 + i14 + i15;
  v3a = i00 + i01 + i04 + i05 + i08 + i09 + i12 + i13;
  v3b = i02 + i03 + i06 + i07 + i10 + i11 + i14 + i15;
  v4a = i00 + i02 + i04 + i06 + i08 + i10 + i12 + i14;
  v4b = i01 + i03 + i05 + i07 + i09 + i11 + i13 + i15;
  Use(i00, i01, i02, i03, i04, i05, i06, i07,
      i08, i09, i10, i11, i12, i13, i14, i15);
  Use(i00, i01, i02, i03, i04, i05, i06, i07,
      i08, i09, i10, i11, i12, i13, i14, i15);
}
