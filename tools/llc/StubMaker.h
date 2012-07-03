#ifndef __STUB_MAKER_H
#define __STUB_MAKER_H

#include "llvm/ADT/SmallVector.h"

namespace llvm {

class Module;
class Triple;
class ELFStub;

// For module M, make all required ELF stubs and insert them into StubList.
void MakeAllStubs(const Module &M,
                  const Triple &T,
                  SmallVectorImpl<ELFStub*> *StubList);
void FreeStubList(SmallVectorImpl<ELFStub*> *StubList);

}

#endif
